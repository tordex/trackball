# Tordex Trackball Firmware

This firmware is developed for the trackball created by me.

## Requirements:

* ISP-IDF v5.5.0
* ESP32-S3
* Movements sensor - PAW3395

## Features

* Wireless with BLE connection
* 4 buttons (Left, Right, Middle, Custom)
* The lock key (used to scroll with mouse or lock pressed buttons)
* Support for vertical and horizontal scrolling
* Indicate state with the on board RGB LED
* 100 Hz pooling rate

#### TODO:
* OLED Screen with information and setup
* Battery level support

## Scrolling with the ball

The lock key is used to scroll:

* Press and hold the lock key, roll the rotate to scroll
* Short click the lock key, roll the rotate to scroll, short click to stop scrolling
* Press any button and short click the lock key. The button will stay pressed when you release the button. Move pointer to the required position and short click to release locked button.

## On Board LED States

* **Blue** - ready for connecting
* **Green** - connected
* **Cyan** - the lock key is active
