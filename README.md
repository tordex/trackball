# Tordex Trackball Firmware

Firmware for a custom ESP32-S3 wireless trackball using a PAW3395 optical sensor and BLE HID.

## Highlights

- BLE HID mouse
- PAW3395 motion sensor (SPI)
- SSD1306 OLED status/config UI (I2C)
- Battery monitoring and BLE battery level reporting
- Vertical and horizontal scrolling
- High-resolution scrolling mode
- DPI presets switchable from hardware button
- Deep sleep on inactivity with GPIO wakeup

## Hardware

- MCU: ESP32-S3
- Sensor: PAW3395
- Display: SSD1306 OLED (I2C address `0x3C`)
- Buttons: 6 total
	- `BTN1`, `BTN2`, `BTN3` (main mouse buttons)
	- `MODE` (scroll mode toggle)
	- `SCROLL` (hold/lock scroll and button lock behavior)
	- `CFG` (DPI preset switch)

Pin mapping is defined in `main/pins.h`.

## Software Requirements

- ESP-IDF `v5.5.x`
- CMake `>= 3.16`

## Build and Flash

1. Open an ESP-IDF environment shell.
2. Set target (first time only):

```bash
idf.py set-target esp32s3
```

3. Build:

```bash
idf.py build
```

4. Flash:

```bash
idf.py -p <PORT> flash
```

5. Optional monitor:

```bash
idf.py -p <PORT> monitor
```

## Runtime Controls

### Pointer

- Roll the trackball to move the pointer.

### Scrolling

- Hold `SCROLL` and roll the ball: temporary scroll mode.
- Click `SCROLL`: toggle locked scroll mode on/off.

### Button Lock (drag lock)

- Press and hold a mouse button.
- Click `SCROLL` to lock that button state.
- Click `SCROLL` again to release all locked buttons.

### DPI Preset

- Click `CFG` to cycle DPI presets.

### Scroll Mode and Hi-Res

- Click `MODE` to cycle scroll axis modes:
	- vertical + horizontal
	- vertical only
	- horizontal only
- Hold `MODE` to toggle high-resolution scrolling.

## Project Structure

- `main/app.cpp`: app state machine, input handling, power management
- `main/paw3395/`: PAW3395 sensor driver
- `main/nimble/`: BLE HID implementation
- `main/oled/`: SSD1306 driver and UI rendering
- `main/battery/`: battery measurement and level updates
- `main/button/`: button handling (click/hold/state)

## Configuration Notes

- Build-time settings are in `sdkconfig` and `sdkconfig.defaults`.
- Project-specific Kconfig entries are in `main/Kconfig.projbuild`.
- The default deep sleep timeout is 3 minutes (`180000 ms`) in app config.

## Troubleshooting

- Build fails with missing IDF tools:
	- verify ESP-IDF is exported correctly in your shell
- No serial port found:
	- check cable/data support and device permissions
- Device not discoverable over BLE:
	- erase/reflash and reset the board
	- confirm power and battery voltage are stable

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

