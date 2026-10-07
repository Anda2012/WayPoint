# WayPoint

> ESP32-based offline navigation prototype for a compact embedded map device.

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32%20S3-FF7A00?logo=platformio)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/ESP32-S3-000000?logo=arduino)](https://www.espressif.com/en/products/socs/esp32-s3)
[![LVGL](https://img.shields.io/badge/LVGL-8-7A6CFF?logo=lvgl)](https://lvgl.io/)
[![Status](https://img.shields.io/badge/status-early%20prototype-orange)](#development-status)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

## Overview

WayPoint is an embedded navigation project intended for the ES3C35P QSPI st77922 based display board with a compact touch display and offline map capabilities. The repository currently contains the initial PlatformIO scaffold and a custom board definition for an ESP32-S3 development board with a 3.5 inch 320x480 display.

At this stage, the codebase is best understood as a foundation for a navigation system rather than a complete, finished product. The actual firmware in this repository is minimal and mostly consists of the default Arduino setup/loop structure, while the board configuration and project metadata describe the target hardware.

## Current repository state

This repository currently includes:

- A PlatformIO project configured for ESP32-S3
- A custom board definition for `FNK0104N_3P5_320x480_ST77922`
- A minimal Arduino sketch in `src/main.cpp`
- Build artifacts generated in `.pio/build/`

The project is under active development and should be considered an early-stage prototype.

## Features

The following are either clearly present or clearly implied by the repository state:

- ESP32-S3 PlatformIO project configuration
- Custom board definition for a 320x480 display target
- Arduino-based firmware skeleton
- Embedded target metadata for a 16MB flash board variant

The following are planned, but not implemented in the current repository:

- GPS acquisition and parsing
- Offline map rendering
- Tile or vector map management
- Navigation logic and routing
- LVGL UI framework integration
- IMU / compass support
- Touch and user input handling

## Hardware

The repository includes a custom board definition at `boards/FNK0104N_3P5_320x480_ST77922.json`.

From that file, the project targets:

- ESP32-S3 MCU
- 16 MB flash
- 320 KB RAM allocation in the board metadata
- 3.5 inch display panel described as `320x480`
- Board name: `FNK0104N 3.5in 320x480 ST77922 ESP32-S3-N16R8V`

The project does not currently contain GPIO pin assignments, display driver code, GPS wiring, or IMU connectivity details in source files. Those will need to be added later when the hardware integration layer is implemented.

## Architecture

```text
ESP32-S3
├── Display controller / panel interface
├── GPS receiver (planned)
├── IMU / compass (planned)
├── Offline map storage (planned)
├── Map rendering layer (planned)
├── Navigation logic (planned)
└── User interface (planned)
```

This is a conceptual architecture based on the intended use case, not a claim that all of these modules are already implemented. The actual code in the repository is still a minimal Arduino skeleton.

## Software architecture

The repository currently follows a simple embedded firmware layout typical of PlatformIO projects:

- `platformio.ini` configures the project target, framework, and board selection
- `boards/FNK0104N_3P5_320x480_ST77922.json` defines the custom board metadata
- `src/main.cpp` contains the initial firmware entry point

There is no implemented navigation stack, no map engine, and no display abstraction code in the current source tree.

## Map system

No actual map engine is present in the codebase at this time.

The following are not implemented in the repository as written:

- Offline raster maps
- Vector tile rendering
- Map caching
- Geographic coordinate transforms
- Route rendering
- Navigation overlays

Because of that, any map-system claims beyond the project concept should be treated as planned work rather than completed functionality.

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
```

The custom board is defined in `boards/FNK0104N_3P5_320x480_ST77922.json` and sets the target MCU and flash configuration for the board variant.

No application-level config file, map configuration, or GPS settings are present in the repository at this time.

## Hardware wiring

No pin assignments or wiring schema are currently defined in the repository. The custom board JSON provides board metadata, but it does not specify GPIO mapping for GPS, display, touch, or other peripherals.

A future hardware section will include a pinout table once those definitions are added to the project.

## Project structure

```text
.
├── boards/
│   └── FNK0104N_3P5_320x480_ST77922.json
├── include/
│   └── README
├── lib/
│   └── README
├── src/
│   └── main.cpp
├── test/
│   └── README
├── .gitignore
├── .pio/
├── platformio.ini
├── README.md
└── README
```

## Development status

| Component | Status |
| --- | --- |
| PlatformIO project scaffold | Completed |
| Custom board config | Completed |
| ESP32-S3 target selection | Completed |
| Arduino firmware skeleton | Completed |
| Display integration | Not yet implemented |
| GPS receiver support | Planned |
| IMU / compass support | Planned |
| Offline map rendering | Planned |
| Navigation logic | Planned |
| LVGL UI | Planned |
| Touch input | Planned |

## Roadmap

### Completed

- Confirmed PlatformIO structure for an ESP32-S3 target
- Added custom board metadata for the display board
- Created a minimal Arduino firmware entry point

### In Progress

- Defining the actual project direction and hardware interfaces
- Establishing the long-term software structure for navigation and UI layers

### Planned

- GPS receiver integration
- Offline map data pipeline
- Display driver setup and screen drawing
- Touch handling
- Map navigation and route rendering
- IMU / compass support
- UI polish and device behavior refinement

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
