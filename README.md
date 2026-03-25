# BLETracker

Copyright (C) 2026 Lancelot MEI

BLETracker is a Zephyr-based firmware project for a Bluetooth Low Energy tracker built around a Nordic nRF52840 platform and a u-blox SAM-M10 GNSS module. The repository also contains the associated board definition files and  schematic design files.

## Repository layout

- `src/`: application firmware, BLE services, GNSS/UBX handling, and hardware glue.
- `boards/IMYTH/BLETracker/`: custom Zephyr board definition for the target hardware.
- `docs/`: project notes and design documents.
- `hardware/`: KiCad source files (converted from EasyCAD, need adjust) and related hardware assets.

## Build notes

This project is intended to be built with Zephyr and a configured Zephyr SDK / toolchain environment.

Typical build flow:

```sh
west build -b BLETracker/nrf52840
```

You may need to adjust the board target or workspace layout to match your local Zephyr setup.

## GitHub readiness notes

- `build/` and local editor state are intentionally ignored.
- `.vscode/` contains machine-local IDE configuration and is not required to build the project.
- `src/ubx_messages_header.h` is a generated u-blox header and retains its original upstream notice.

## License

Unless a file states otherwise, this repository is licensed under `GPL-2.0-only`. See [LICENSE](LICENSE).

Third-party or generated files may carry their own notices and should be treated according to the license text embedded in those files.
