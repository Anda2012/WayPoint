#include <Arduino.h>
#include <SD_MMC.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <PNGdec.h>
#include <vector>
#include <string>
#include <algorithm>
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "TFT_Drivers/ST77922/ST77922_Touch.h"

#if LV_COLOR_DEPTH != 16
#error "LV_COLOR_DEPTH must be 16 for this example"
#endif

#define SCREEN_ROTATION 0
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 480
#define BUF_LINES 40
#define FILES_PER_PAGE 6

TFT_eSPI tft = TFT_eSPI();
ST77922_TOUCH touch;

static lv_disp_draw_buf_t drawBuffer;
static lv_color_t lvBuffer1[SCREEN_WIDTH * BUF_LINES];
static lv_color_t lvBuffer2[SCREEN_WIDTH * BUF_LINES];
static uint16_t *composeBuffer;

static lv_obj_t *statusLabel = nullptr;
static lv_obj_t *listContainer = nullptr;
static String currentPath = "/";
static size_t currentPageOffset = 0;
static volatile bool imageViewerOpen = false;
static int currentViewerX = 0;
static int currentViewerY = 0;
static bool pngOpenRequested = false;
static bool viewerTouchWasDown = false;
static String requestedPngPath;
static volatile bool pngDecodeRunning = false;
static File pngFile;
static PNG png;

static void buildUi(void);
static void decodePngTask(void *argument);

struct DirEntry {
  std::string name;
  std::string fullPath;
  bool isDirectory;
};

void *pngOpen(const char *filename, int32_t *size)
{
  pngFile = SD_MMC.open(filename, FILE_READ);
  if (!pngFile) {
    return nullptr;
  }

  *size = pngFile.size();
  return &pngFile;
}

void pngClose(void *handle)
{
  File *file = (File *)handle;
  if (file && *file) {
    file->close();
  }
}

int32_t pngRead(PNGFILE *page, uint8_t *buffer, int32_t length)
{
  if (!pngFile) {
    return 0;
  }

  (void)page;
  return pngFile.read(buffer, length);
}

int32_t pngSeek(PNGFILE *page, int32_t position)
{
  if (!pngFile) {
    return 0;
  }

  (void)page;
  return pngFile.seek(position) ? 1 : 0;
}

int pngDraw(PNGDRAW *pDraw)
{
  const uint16_t lineWidth = min((uint16_t)SCREEN_WIDTH, (uint16_t)pDraw->iWidth);
  uint16_t lineBuffer[SCREEN_WIDTH];

  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xFFFFFFFF);
  tft.pushImage(currentViewerX, currentViewerY + pDraw->y, lineWidth, 1, lineBuffer);
  vTaskDelay(1);
  return 1;
}

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

static void drawViewerExitButton(void)
{
  const int buttonX = SCREEN_WIDTH - 74;
  const int buttonY = 8;
  const int buttonW = 60;
  const int buttonH = 28;

  tft.fillRoundRect(buttonX, buttonY, buttonW, buttonH, 8, TFT_RED);
  tft.drawRoundRect(buttonX, buttonY, buttonW, buttonH, 8, TFT_LIGHTGREY);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.drawString("Exit", buttonX + 15, buttonY + 7, 2);
}

static void readTouch(lv_indev_drv_t *input, lv_indev_data_t *data)
{
  if (touch.Get_Touch()) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touch.touch.x[0];
    data->point.y = touch.touch.y[0];
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

static String parentPath(const String &path)
{
  if (path == "/") {
    return "/";
  }

  String parent = path;
  parent = parent.substring(0, parent.lastIndexOf('/'));
  if (parent.length() == 0) {
    return "/";
  }
  if (!parent.endsWith("/")) {
    return parent;
  }
  return parent;
}

static void countDirectoryEntries(size_t &directoryCount, size_t &fileCount)
{
  directoryCount = 0;
  fileCount = 0;
  File root = SD_MMC.open(currentPath);
  if (!root || !root.isDirectory()) {
    Serial.printf("Unable to open directory: %s\n", currentPath.c_str());
    return;
  }

  while (File entry = root.openNextFile()) {
    if (entry.isDirectory()) {
      ++directoryCount;
    } else {
      ++fileCount;
    }
    entry.close();
  }
  root.close();
}

static void decodePngViewer(const String &path)
{
  if (!path.endsWith(".png") && !path.endsWith(".PNG")) {
    pngDecodeRunning = false;
    vTaskDelete(nullptr);
    return;
  }

  tft.fillScreen(TFT_BLACK);
  int result = png.open(path.c_str(), pngOpen, pngClose, pngRead, pngSeek, pngDraw);
  if (!pngFile || result != PNG_SUCCESS) {
    Serial.printf("PNG open failed (decoder=%d, file=%s)\n",
                  result, pngFile ? "open" : "closed");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("PNG open failed", 20, 20, 2);
    if (pngFile) {
      png.close();
    }
  } else {
    const int imageWidth = png.getWidth();
    const int imageHeight = png.getHeight();
    if (imageWidth <= 0 || imageWidth > SCREEN_WIDTH ||
        imageHeight <= 0 || imageHeight > SCREEN_HEIGHT) {
      Serial.printf("PNG dimensions not supported: %d x %d\n", imageWidth, imageHeight);
      png.close();
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("PNG size unsupported", 20, 20, 2);
    } else {
      Serial.printf("Opening PNG: %s (%d x %d), free heap=%u, largest=%u\n",
                    path.c_str(), imageWidth, imageHeight,
                    (unsigned)ESP.getFreeHeap(),
                    (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
      currentViewerX = (SCREEN_WIDTH - imageWidth) / 2;
      currentViewerY = (SCREEN_HEIGHT - imageHeight) / 2;

      tft.startWrite();
      result = png.decode(NULL, 0);
      png.close();
      tft.endWrite();

      if (result != PNG_SUCCESS) {
        Serial.printf("PNG decode failed with error %d\n", result);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("PNG decode failed", 20, 20, 2);
      }
    }
  }

  viewerTouchWasDown = false;
  drawViewerExitButton();
  Serial.printf("PNG task stack high-water mark: %u bytes\n",
                (unsigned)uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t));
  pngDecodeRunning = false;
  vTaskDelete(nullptr);
}

static void decodePngTask(void *argument)
{
  const String path = *static_cast<String *>(argument);
  decodePngViewer(path);
}

static void populateFileList(void)
{
  if (listContainer == nullptr) {
    return;
  }

  lv_obj_clean(listContainer);

  String labelText = "Path: ";
  labelText += currentPath;
  if (statusLabel != nullptr) {
    lv_label_set_text(statusLabel, labelText.c_str());
  }

  size_t directoryCount;
  size_t fileCount;
  countDirectoryEntries(directoryCount, fileCount);
  const size_t totalCount = directoryCount + fileCount;
  if (totalCount == 0) {
    currentPageOffset = 0;
  } else if (currentPageOffset >= totalCount) {
    currentPageOffset = ((totalCount - 1) / FILES_PER_PAGE) * FILES_PER_PAGE;
  }

  if (currentPath != "/") {
    lv_obj_t *backBtn = lv_btn_create(listContainer);
    lv_obj_set_size(backBtn, lv_pct(100), 36);
    lv_obj_add_event_cb(backBtn, [](lv_event_t *event) {
      currentPath = parentPath(currentPath);
      currentPageOffset = 0;
      populateFileList();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *backLabel = lv_label_create(backBtn);
    lv_obj_center(backLabel);
    lv_label_set_text(backLabel, "..  Back");
  }

  std::vector<DirEntry> pageEntries;
  pageEntries.reserve(FILES_PER_PAGE);
  for (int directoryPass = 1; directoryPass >= 0; --directoryPass) {
    const bool wantDirectories = directoryPass != 0;
    const size_t groupOffset = wantDirectories ? 0 : directoryCount;
    size_t groupIndex = 0;
    File root = SD_MMC.open(currentPath);
    if (!root || !root.isDirectory()) {
      Serial.printf("Unable to list directory: %s\n", currentPath.c_str());
      break;
    }

    while (File entry = root.openNextFile()) {
      const bool isDirectory = entry.isDirectory();
      if (isDirectory == wantDirectories) {
        const size_t itemIndex = groupOffset + groupIndex++;
        if (itemIndex >= currentPageOffset &&
            itemIndex < currentPageOffset + FILES_PER_PAGE) {
          DirEntry item;
          item.name = entry.name();
          item.fullPath = (currentPath == "/")
                              ? (String("/") + entry.name()).c_str()
                              : (String(currentPath + "/" + entry.name())).c_str();
          item.isDirectory = isDirectory;
          pageEntries.push_back(item);
        }
      }
      entry.close();
    }
    root.close();
  }

  std::sort(pageEntries.begin(), pageEntries.end(), [](const DirEntry &a, const DirEntry &b) {
    if (a.isDirectory != b.isDirectory) {
      return a.isDirectory > b.isDirectory;
    }
    return a.name < b.name;
  });

  for (auto &entry : pageEntries) {
    lv_obj_t *row = lv_btn_create(listContainer);
    lv_obj_set_size(row, lv_pct(100), 38);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x1d2a36), 0);
    lv_obj_set_style_border_width(row, 0, 0);

    auto *data = new DirEntry(entry);
    lv_obj_add_event_cb(row, [](lv_event_t *event) {
      auto *entry = static_cast<DirEntry *>(lv_event_get_user_data(event));
      if (!entry) {
        return;
      }

      String pathText = entry->fullPath.c_str();
      if (entry->isDirectory) {
        currentPath = pathText;
        currentPageOffset = 0;
        populateFileList();
        return;
      }

      if (pathText.endsWith(".png") || pathText.endsWith(".PNG")) {
        requestedPngPath = pathText;
        pngOpenRequested = true;
      }
    }, LV_EVENT_CLICKED, data);
    lv_obj_add_event_cb(row, [](lv_event_t *event) {
      delete static_cast<DirEntry *>(lv_event_get_user_data(event));
    }, LV_EVENT_DELETE, data);

    lv_obj_t *label = lv_label_create(row);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_hex(entry.isDirectory ? 0x8EE3FF : 0xD8E2EC), 0);
    lv_label_set_text(label, (std::string(entry.isDirectory ? "[DIR] " : "[FILE] ") + entry.name).c_str());
  }

  if (totalCount > FILES_PER_PAGE) {
    char pageText[40];
    const size_t pageNumber = currentPageOffset / FILES_PER_PAGE + 1;
    const size_t pageCount = (totalCount + FILES_PER_PAGE - 1) / FILES_PER_PAGE;
    snprintf(pageText, sizeof(pageText), "Page %u / %u",
             (unsigned)pageNumber, (unsigned)pageCount);

    lv_obj_t *pageLabel = lv_label_create(listContainer);
    lv_obj_set_width(pageLabel, lv_pct(100));
    lv_obj_set_style_text_align(pageLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(pageLabel, pageText);

    if (currentPageOffset > 0) {
      lv_obj_t *previousBtn = lv_btn_create(listContainer);
      lv_obj_set_size(previousBtn, lv_pct(100), 36);
      lv_obj_add_event_cb(previousBtn, [](lv_event_t *event) {
        (void)event;
        currentPageOffset = currentPageOffset >= FILES_PER_PAGE
                                ? currentPageOffset - FILES_PER_PAGE
                                : 0;
        populateFileList();
      }, LV_EVENT_CLICKED, nullptr);
      lv_obj_t *previousLabel = lv_label_create(previousBtn);
      lv_obj_center(previousLabel);
      lv_label_set_text(previousLabel, "Previous");
    }

    if (currentPageOffset + FILES_PER_PAGE < totalCount) {
      lv_obj_t *nextBtn = lv_btn_create(listContainer);
      lv_obj_set_size(nextBtn, lv_pct(100), 36);
      lv_obj_add_event_cb(nextBtn, [](lv_event_t *event) {
        (void)event;
        currentPageOffset += FILES_PER_PAGE;
        populateFileList();
      }, LV_EVENT_CLICKED, nullptr);
      lv_obj_t *nextLabel = lv_label_create(nextBtn);
      lv_obj_center(nextLabel);
      lv_label_set_text(nextLabel, "Next");
    }
  }
}

static void buildUi(void)
{
  lv_obj_t *screen = lv_scr_act();
  lv_obj_clean(screen);

  lv_obj_t *title = lv_label_create(screen);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
  lv_label_set_text(title, "WayPoint");

  statusLabel = lv_label_create(screen);
  lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 58);
  lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(statusLabel, lv_color_hex(0xA0AEC0), 0);
  lv_label_set_text(statusLabel, "Path: /");

  listContainer = lv_obj_create(screen);
  lv_obj_set_size(listContainer, 280, 330);
  lv_obj_align(listContainer, LV_ALIGN_TOP_MID, 0, 90);
  lv_obj_set_style_bg_color(listContainer, lv_color_hex(0x111827), 0);
  lv_obj_set_style_radius(listContainer, 16, 0);
  lv_obj_set_style_pad_all(listContainer, 8, 0);
  lv_obj_set_style_pad_gap(listContainer, 6, 0);
  lv_obj_set_flex_flow(listContainer, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(listContainer, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(listContainer, LV_OBJ_FLAG_SCROLLABLE);

  populateFileList();
}

void setup()
{
  Serial.begin(115200);
  Serial.printf("Reset reason: %d\n", (int)esp_reset_reason());

  if (!SD_MMC.setPins(5, 4, 6, 7, 2, 3)) {
    Serial.println("SD_MMC setPins failed");
  }

  if (!SD_MMC.begin("/sdcard")) {
    Serial.println("SD_MMC begin failed");
  } else {
    Serial.println("SD_MMC init ok");
  }

  lv_init();

  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.setSwapBytes(false);

  if (tft.width() != SCREEN_WIDTH || tft.height() != SCREEN_HEIGHT) {
    Serial.println("FATAL: ST77922 rotation is not 320x480 portrait");
    while (true) {
      delay(1000);
    }
  }

  composeBuffer = (uint16_t *)heap_caps_malloc(
      (size_t)SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t),
      MALLOC_CAP_SPIRAM);
  if (composeBuffer == NULL) {
    Serial.println("WARNING: PSRAM allocation failed; using internal RAM");
    composeBuffer = (uint16_t *)heap_caps_malloc(
        (size_t)SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t),
        MALLOC_CAP_8BIT);
  }
  if (composeBuffer == NULL) {
    Serial.println("FATAL: unable to allocate LVGL compose buffer");
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
  esp_timer_create(&timerArguments, &timer);
  esp_timer_start_periodic(timer, 1000);

  buildUi();
}

void loop()
{
  if (pngOpenRequested) {
    pngOpenRequested = false;
    imageViewerOpen = true;
    pngDecodeRunning = true;
    viewerTouchWasDown = false;
    if (xTaskCreatePinnedToCore(
            decodePngTask, "png_decode", 24 * 1024,
            &requestedPngPath, 1, nullptr, 1) != pdPASS) {
      Serial.printf("Unable to create PNG decoder task; free heap=%u\n",
                    (unsigned)ESP.getFreeHeap());
      pngDecodeRunning = false;
      imageViewerOpen = false;
      buildUi();
    }
  }

  if (imageViewerOpen) {
    const bool touchDown = !pngDecodeRunning && touch.Get_Touch();
    if (touchDown && !viewerTouchWasDown) {
      const int x = touch.touch.x[0];
      const int y = touch.touch.y[0];
      const int buttonX = SCREEN_WIDTH - 74;
      const int buttonY = 8;
      const int buttonW = 60;
      const int buttonH = 28;

      if (x >= buttonX && x <= buttonX + buttonW &&
          y >= buttonY && y <= buttonY + buttonH) {
        imageViewerOpen = false;
        buildUi();
      }
    }
    viewerTouchWasDown = touchDown;
    delay(5);
    return;
  }

  lv_timer_handler();
  delay(5);
}