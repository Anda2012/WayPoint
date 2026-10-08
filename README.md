# WayPoint

> ESP32-based offline navigation prototype for a compact embedded map device.

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32%20S3-FF7A00?logo=platformio)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/ESP32-S3-000000?logo=arduino)](https://www.espressif.com/en/products/socs/esp32-s3)
[![LVGL](https://img.shields.io/badge/LVGL-8-7A6CFF?logo=lvgl)](https://lvgl.io/)
[![Status](https://img.shields.io/badge/status-early%20prototype-orange)](#development-status)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

## Overview

WayPoint is an embedded navigation prototype for an ESP32-S3 board with a 320x480 ST77922 display. The current firmware initializes LVGL 8, mounts the microSD card over SD_MMC, and displays a fixed 3x3 grid of offline map tiles with a marker at a configured geographic coordinate.

The map center is currently a compile-time sample coordinate, not a live GPS fix. GPS acquisition, map panning, and route navigation are not implemented yet.

## Current repository state

This repository currently includes:

- A PlatformIO project configured for ESP32-S3
- A custom board definition for `FNK0104N_3P5_320x480_ST77922`
- LVGL display and touch initialization
- SD_MMC mounting using the configured SDIO pins
- A fixed-grid offline map tile renderer using `map_tiles_lvgl8`

The project is under active development and should be considered an early-stage prototype.

## Features

Implemented in the current firmware:

- ESP32-S3 PlatformIO project configuration
- Custom board definition for a 320x480 display target
- Arduino firmware using LVGL 8
- SD_MMC-backed fixed 3x3 map tile display
- A marker centered on the selected latitude/longitude

Not yet implemented:

- GPS acquisition and parsing
- Dynamic map positioning, panning, and zoom controls
- Navigation logic and routing
- IMU / compass support

## Hardware

The repository includes a custom board definition at `boards/FNK0104N_3P5_320x480_ST77922.json`.

From that file, the project targets:

- ESP32-S3 MCU
- 16 MB flash
- 320 KB RAM allocation in the board metadata
- 3.5 inch display panel described as `320x480`
- Board name: `FNK0104N 3.5in 320x480 ST77922 ESP32-S3-N16R8V`

The firmware configures the SD card as 4-bit SDIO. Display GPIO details are defined by the TFT_eSPI board setup and touch driver; GPS and IMU wiring are not implemented.

## Architecture

```text
ESP32-S3
├── Display controller / panel interface
├── 4-bit SD_MMC ── map_tiles_lvgl8 fixed tile grid
├── ST77922 display ── TFT_eSPI flush callback
├── Touch controller ── LVGL pointer input
└── GPS / navigation / IMU (planned)
```

The current map position is a fixed sample coordinate; it is not yet connected to a GPS receiver.

## Software architecture

The PlatformIO firmware combines:

- `platformio.ini` configures the project target, framework, and board selection
- `boards/FNK0104N_3P5_320x480_ST77922.json` defines the custom board metadata
- `src/main.cpp` contains display/touch setup, SD_MMC configuration, and the fixed-grid tile example
- `map_tiles_lvgl8` provides LVGL 8 tile descriptors, coordinate conversion, and tile file loading

The current renderer loads a 3x3 set of tiles around the configured coordinate and positions that point at the display center.

## Map system

The firmware uses the `map_tiles_*` fixed-grid API from `map_tiles_lvgl8`. It loads up to nine 256x256 RGB565 tiles from the microSD card and places an LVGL marker at the configured coordinate. The map view is fixed: live GPS updates, panning, zoom controls, and route rendering are not wired into this firmware.

Tile files must be converted to the library's expected format (12-byte header followed by 256x256 RGB565 pixel data) and stored on the card using this layout:

```text
/tiles/<zoom>/<x>/<y>.bin
```

The firmware defaults to zoom 16 and the center of the provided BKK–DMK area, `13.79152, 100.63236`. Update `MAP_LATITUDE` and `MAP_LONGITUDE` in `src/main.cpp` to change the map center. The 3x3 tile buffers require PSRAM.

## Installation

1. Clone the repository:

```bash
git clone <repository-url>
cd WayPoint
```

2. Install PlatformIO:

```bash
python -m pip install platformio
```

3. Ensure the project environment is available:

```bash
pio project init
```

4. Build the project:

```bash
pio run -e FNK0104N_3P5_320x480_ST77922
```

5. Upload to the board:

```bash
pio run -e FNK0104N_3P5_320x480_ST77922 -t upload
```

6. Monitor serial output:

```bash
pio device monitor
```

## Configuration

The primary project configuration is in `platformio.ini`.

Current configuration:

```ini
[env:FNK0104N_3P5_320x480_ST77922]
platform = espressif32
board = FNK0104N_3P5_320x480_ST77922
framework = arduino
lib_deps =
    lvgl/lvgl@^8.3.11
    https://github.com/Anda2012/map_tiles_lvgl8_arduino.git
    TFT_eSPI
```

The tile grid, zoom level, storage folder, and sample center coordinate are configured by the `MAP_GRID`, `MAP_ZOOM`, `TILE_FOLDER`, `MAP_LATITUDE`, and `MAP_LONGITUDE` constants in `src/main.cpp`. The tile base path is empty because the library is connected directly to `SD_MMC` with `map_tiles_use_arduino_fs`.

## Hardware wiring

The SD_MMC pins are configured in `src/main.cpp`:

| SDIO signal | GPIO |
| --- | ---: |
| CLK | 5 |
| CMD | 4 |
| D0 | 6 |
| D1 | 7 |
| D2 | 2 |
| D3 | 3 |

Display and touch pins are configured by the TFT_eSPI setup and ST77922 touch driver. GPS and IMU wiring is not yet defined.

## Project structure

```text
.
├── boards/
│   └── FNK0104N_3P5_320x480_ST77922.json
├── include/
├── lib/
│   ├── TFT_eSPI/
│   └── lvgl/
├── src/
│   └── main.cpp
├── .gitignore
├── platformio.ini
├── README.md
└── LICENSE
```

## Development status

| Component | Status |
| --- | --- |
| PlatformIO project scaffold | Completed |
| Custom board config | Completed |
| ESP32-S3 target selection | Completed |
| Arduino firmware skeleton | Completed |
| Display integration | Implemented with TFT_eSPI |
| Touch input | Initialized as an LVGL pointer |
| SD_MMC tile loading | Implemented |
| Fixed 3x3 map tile display | Implemented; needs matching tile files |
| GPS receiver support | Planned |
| IMU / compass support | Planned |
| Navigation logic | Planned |
| Live map movement and zoom | Planned |

## Roadmap

### Completed

- Confirmed PlatformIO structure for an ESP32-S3 target
- Added custom board metadata for the display board
- Initialized the TFT_eSPI display and LVGL 8
- Added SD_MMC loading for the fixed 3x3 map tile grid

### In Progress

- Validate the fixed-grid renderer with the project's prepared SD tile set and hardware

### Planned

- GPS receiver integration
- Live map positioning, panning, and zoom
- Route rendering and navigation
- IMU / compass support
- UI refinement and device behavior testing

## Screenshots

> Screenshots coming soon.

No screenshots or media assets are present in the repository at this time.

## Contributing

Contributions are welcome once the project direction and architecture are formalized. At this stage, the project is still in the early design / platform setup phase.

Suggested areas for contribution:

- GPS and serial protocol integration
- Embedded display driver setup
- Map data handling and rendering
- Navigation logic and state management
- UI architecture and control flow

If you want to contribute, open a pull request with a clear description of the hardware target and the exact functionality under development.

## Issues and pull requests

This repository does not yet include a formal contribution policy or a project issue workflow beyond the GitHub-native issue and pull request tools.

## License

This project is licensed under the [MIT License](LICENSE).

Copyright (c) 2026 WayPoint contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Acknowledgements

- PlatformIO for the embedded build environment
- Espressif for the ESP32-S3 platform
- The open-source embedded and navigation communities that inform this project direction

## Notes

This repository currently represents the beginning of a WayPoint implementation rather than a complete navigation system. The code is intentionally conservative and does not claim features that are not actually present in the source tree.
