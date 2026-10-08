#include <Arduino.h>
#include <SD_MMC.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <MapTiles.h>
#include <MapTilesArduinoFS.h>
#include <limits.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "TFT_Drivers/ST77922/ST77922_Touch.h"

#if LV_COLOR_DEPTH != 16
#error "LV_COLOR_DEPTH must be 16 for this example"
#endif

#define SCREEN_ROTATION 0
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 480
#define BUF_LINES 40
#define MAP_GRID 3
#define MAP_ZOOM 16
#define TILE_BASE_PATH ""
#define TILE_FOLDER "tiles"

static const double MAP_LATITUDE = 13.79152;
static const double MAP_LONGITUDE = 100.63236;

TFT_eSPI tft = TFT_eSPI();
ST77922_TOUCH touch;

static lv_disp_draw_buf_t drawBuffer;
static lv_color_t lvBuffer1[SCREEN_WIDTH * BUF_LINES];
static lv_color_t lvBuffer2[SCREEN_WIDTH * BUF_LINES];
static uint16_t *composeBuffer;

static map_tiles_handle_t mapHandle = nullptr;
static lv_obj_t *mapContainer = nullptr;
static lv_obj_t *tileImages[MAP_GRID * MAP_GRID];
static lv_obj_t *positionMarker = nullptr;
static lv_obj_t *statusLabel = nullptr;
static double mapLatitude = MAP_LATITUDE;
static double mapLongitude = MAP_LONGITUDE;
static int loadedBaseX = INT_MIN;
static int loadedBaseY = INT_MIN;
static bool touchIsDown = false;
static int previousTouchX = 0;
static int previousTouchY = 0;
static bool sampledTouchDown = false;
static int sampledTouchX = 0;
static int sampledTouchY = 0;

static void updateMapPosition(void);

static uint16_t swapRedBlue(uint16_t value)
{
  return (uint16_t)((value & 0x07E0) |
                    ((value & 0xF800) >> 11) |
                    ((value & 0x001F) << 11));
}

static void flushDisplay(lv_disp_drv_t *display, const lv_area_t *area, lv_color_t *color)
{
  const uint32_t width = (uint32_t)(area->x2 - area->x1 + 1);
  const uint32_t height = (uint32_t)(area->y2 - area->y1 + 1);
  const uint16_t *source = (const uint16_t *)color;

  for (uint32_t row = 0; row < height; ++row) {
    uint32_t destination = (uint32_t)(area->y1 + row) * SCREEN_WIDTH + area->x1;
    for (uint32_t column = 0; column < width; ++column) {
      uint16_t value = swapRedBlue(source[row * width + column]);
      composeBuffer[destination + column] = __builtin_bswap16(value);
    }
  }

  if (lv_disp_flush_is_last(display)) {
    tft.pushImage(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, composeBuffer);
  }

  lv_disp_flush_ready(display);
}

static void readTouch(lv_indev_drv_t *input, lv_indev_data_t *data)
{
  if (sampledTouchDown) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = sampledTouchX;
    data->point.y = sampledTouchY;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }

  (void)input;
}

static void lvglTick(void *argument)
{
  (void)argument;
  lv_tick_inc(1);
}

static void setStatus(const char *text)
{
  Serial.println(text);
  if (statusLabel != nullptr) {
    lv_label_set_text(statusLabel, text);
  }
}

static void setupDisplay(void)
{
  lv_init();

  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.setSwapBytes(false);

  if (tft.width() != SCREEN_WIDTH || tft.height() != SCREEN_HEIGHT) {
    Serial.println("FATAL: display rotation is not 320x480 portrait");
    while (true) {
      delay(1000);
    }
  }

  composeBuffer = (uint16_t *)heap_caps_malloc(
      (size_t)SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t),
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (composeBuffer == nullptr) {
    Serial.println("FATAL: unable to allocate display compose buffer in PSRAM");
    while (true) {
      delay(1000);
    }
  }
  memset(composeBuffer, 0, (size_t)SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t));

  lv_disp_draw_buf_init(&drawBuffer, lvBuffer1, lvBuffer2, SCREEN_WIDTH * BUF_LINES);

  static lv_disp_drv_t displayDriver;
  lv_disp_drv_init(&displayDriver);
  displayDriver.hor_res = SCREEN_WIDTH;
  displayDriver.ver_res = SCREEN_HEIGHT;
  displayDriver.flush_cb = flushDisplay;
  displayDriver.draw_buf = &drawBuffer;
  lv_disp_drv_register(&displayDriver);

  touch.init();
  touch.Set_Rotation(SCREEN_ROTATION);

  static lv_indev_drv_t inputDriver;
  lv_indev_drv_init(&inputDriver);
  inputDriver.type = LV_INDEV_TYPE_POINTER;
  inputDriver.read_cb = readTouch;
  lv_indev_drv_register(&inputDriver);

  const esp_timer_create_args_t timerArguments = {
      .callback = &lvglTick,
      .name = "lvgl_tick"};
  esp_timer_handle_t timer;
  ESP_ERROR_CHECK(esp_timer_create(&timerArguments, &timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 1000));

  statusLabel = lv_label_create(lv_scr_act());
  lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_text_color(statusLabel, lv_color_white(), 0);
  lv_obj_set_style_bg_color(statusLabel, lv_color_hex(0x111827), 0);
  lv_obj_set_style_bg_opa(statusLabel, LV_OPA_80, 0);
  lv_obj_set_style_pad_all(statusLabel, 5, 0);
  lv_label_set_text(statusLabel, "Initializing map...");
}

static void updateMapPosition(void)
{
  map_tiles_set_center_from_gps(mapHandle, mapLatitude, mapLongitude);

  int baseX;
  int baseY;
  map_tiles_get_position(mapHandle, &baseX, &baseY);
  const bool tileWindowChanged = baseX != loadedBaseX || baseY != loadedBaseY;
  int loadedTiles = 0;

  if (tileWindowChanged) {
    for (int i = 0; i < MAP_GRID * MAP_GRID; ++i) {
      const int column = i % MAP_GRID;
      const int row = i / MAP_GRID;
      if (map_tiles_load_tile(mapHandle, i, baseX + column, baseY + row)) {
        lv_img_set_src(tileImages[i], map_tiles_get_image(mapHandle, i));
        lv_obj_clear_flag(tileImages[i], LV_OBJ_FLAG_HIDDEN);
        ++loadedTiles;
      } else {
        lv_obj_add_flag(tileImages[i], LV_OBJ_FLAG_HIDDEN);
      }
    }
    loadedBaseX = baseX;
    loadedBaseY = baseY;
  } else {
    for (int i = 0; i < MAP_GRID * MAP_GRID; ++i) {
      if (!lv_obj_has_flag(tileImages[i], LV_OBJ_FLAG_HIDDEN)) {
        ++loadedTiles;
      }
    }
  }

  double tileX;
  double tileY;
  map_tiles_gps_to_tile_xy(mapHandle, mapLatitude, mapLongitude, &tileX, &tileY);
  const int markerX = (int)((tileX - baseX) * MAP_TILES_TILE_SIZE);
  const int markerY = (int)((tileY - baseY) * MAP_TILES_TILE_SIZE);
  lv_obj_set_pos(positionMarker, markerX - 7, markerY - 7);
  lv_obj_set_pos(mapContainer,
                 SCREEN_WIDTH / 2 - markerX,
                 SCREEN_HEIGHT / 2 - markerY);

  char status[56];
  snprintf(status, sizeof(status), "Zoom %d | %d/%d tiles | drag to pan",
           MAP_ZOOM, loadedTiles, MAP_GRID * MAP_GRID);
  lv_label_set_text(statusLabel, status);
  lv_obj_move_foreground(statusLabel);

  if (tileWindowChanged) {
    Serial.printf("Map center %.6f, %.6f; base tile %d,%d; free PSRAM %u bytes\n",
                  mapLatitude, mapLongitude, baseX, baseY,
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  }
}

static void pollTouchAndPan(void)
{
  sampledTouchDown = touch.Get_Touch();
  if (sampledTouchDown) {
    sampledTouchX = touch.touch.x[0];
    sampledTouchY = touch.touch.y[0];

    if (touchIsDown) {
      const int deltaX = sampledTouchX - previousTouchX;
      const int deltaY = sampledTouchY - previousTouchY;
      if (deltaX != 0 || deltaY != 0) {
        double tileX;
        double tileY;
        map_tiles_gps_to_tile_xy(mapHandle, mapLatitude, mapLongitude, &tileX, &tileY);
        tileX -= (double)deltaX / MAP_TILES_TILE_SIZE;
        tileY -= (double)deltaY / MAP_TILES_TILE_SIZE;
        map_tiles_tile_xy_to_gps(mapHandle, tileX, tileY, &mapLatitude, &mapLongitude);
        updateMapPosition();
      }
    }

    previousTouchX = sampledTouchX;
    previousTouchY = sampledTouchY;
    touchIsDown = true;
  } else {
    touchIsDown = false;
  }
}

static bool setupMap(void)
{
  if (!SD_MMC.setPins(5, 4, 6, 7, 2, 3)) {
    setStatus("SD_MMC pin setup failed");
    return false;
  }
  if (!SD_MMC.begin("/sdcard")) {
    setStatus("SD card mount failed");
    return false;
  }

  map_tiles_use_arduino_fs(SD_MMC);

  map_tiles_config_t config;
  memset(&config, 0, sizeof(config));
  config.base_path = TILE_BASE_PATH;
  config.tile_folders[0] = TILE_FOLDER;
  config.tile_type_count = 1;
  config.default_tile_type = 0;
  config.default_zoom = MAP_ZOOM;
  config.grid_cols = MAP_GRID;
  config.grid_rows = MAP_GRID;
  config.use_spiram = true;

  mapHandle = map_tiles_init(&config);
  if (mapHandle == nullptr) {
    setStatus("Map tile initialization failed");
    return false;
  }

  lv_obj_t *screen = lv_scr_act();
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  mapContainer = lv_obj_create(screen);
  lv_obj_set_size(mapContainer,
                  MAP_GRID * MAP_TILES_TILE_SIZE,
                  MAP_GRID * MAP_TILES_TILE_SIZE);
  lv_obj_set_style_pad_all(mapContainer, 0, 0);
  lv_obj_set_style_border_width(mapContainer, 0, 0);
  lv_obj_set_style_radius(mapContainer, 0, 0);
  lv_obj_set_style_bg_opa(mapContainer, LV_OPA_TRANSP, 0);
  lv_obj_clear_flag(mapContainer, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < MAP_GRID * MAP_GRID; ++i) {
    tileImages[i] = lv_img_create(mapContainer);
    lv_obj_set_pos(tileImages[i],
                   (i % MAP_GRID) * MAP_TILES_TILE_SIZE,
                   (i / MAP_GRID) * MAP_TILES_TILE_SIZE);
    lv_obj_add_flag(tileImages[i], LV_OBJ_FLAG_HIDDEN);
  }

  positionMarker = lv_obj_create(mapContainer);
  lv_obj_set_size(positionMarker, 14, 14);
  lv_obj_set_style_radius(positionMarker, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(positionMarker, lv_color_hex(0xE03B24), 0);
  lv_obj_set_style_border_color(positionMarker, lv_color_white(), 0);
  lv_obj_set_style_border_width(positionMarker, 2, 0);
  lv_obj_clear_flag(positionMarker, LV_OBJ_FLAG_SCROLLABLE);

  updateMapPosition();
  return true;
}

void setup()
{
  Serial.begin(115200);
  setupDisplay();

  if (setupMap()) {
    Serial.println("Tile grid ready");
  }
}

void loop()
{
  if (mapHandle != nullptr) {
    pollTouchAndPan();
  }
  lv_timer_handler();
  delay(5);
}
