# Saildrop-os

Saildrop OS is an ESP32 operating system for sailing gauges and tools.

This is a work in progress project, and currently I'm targeting the Waveshare Esp32 with 1.28 inch touch LCD, connected to an NMEA3WIFI multiplexer using wifi.

[![Watch the Short](https://img.youtube.com/vi/UroC3OnLnkQ/0.jpg)](https://www.youtube.com/shorts/UroC3OnLnkQ)

## Supported Devices

| Device | Display | Touch | Status |
|--------|---------|-------|--------|
| Waveshare ESP32-S3 1.28" Touch LCD | 240x240 GC9A01 (SPI) | CST816S | ✅ Supported |
| Waveshare ESP32-S3 4" Touch LCD | 480x480 ST7701 (RGB) | GT911 | ✅ Supported |

## Customization

The OS is customizable by editing `saildrop/conf.h`.


## Building

https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28

It provides a package with fixed version libraries, use their tft.

- esp32 version 2.0.12
- lvgl 9.3.0
- tft_espi
- micronmea

https://github.com/lvgl/lvgl/blob/v8.3.11/examples
https://docs.lvgl.io/8.4/widgets/extra/index.html

```bash
# Build for your board
make compile BOARD=BOARD_<NAME>

# Upload and monitor
make upload
make monitor
```

## Adding Support for a New Device

To add support for a new ESP32 display board, follow these steps:

### 1. Create a Board Configuration File

Create a new file `saildrop/boards/board_<name>.h` with hardware-specific definitions:

```c
#ifndef BOARD_<NAME>_H
#define BOARD_<NAME>_H

#define BOARD_NAME "Your-Board-Name"

// Screen dimensions
#define SCREEN_WIDTH  <width>
#define SCREEN_HEIGHT <height>

// Display shape (0 = rectangular/square, 1 = circular/round)
#define DISPLAY_IS_ROUND <0 or 1>

// Display driver selection
#define DISPLAY_DRIVER_SPI      <0 or 1>   // For SPI displays (GC9A01, ST7789, etc.)
#define DISPLAY_DRIVER_RGB      <0 or 1>   // For RGB parallel displays (ST7701, etc.)
#define DISPLAY_USE_TFT_ESPI    <0 or 1>   // Use TFT_eSPI library

// Touch driver selection
#define TOUCH_DRIVER_CST816S    <0 or 1>
#define TOUCH_DRIVER_GT911      <0 or 1>

// Touch I2C pins
#define TOUCH_SDA_PIN   <pin>
#define TOUCH_SCL_PIN   <pin>
#define TOUCH_RST_PIN   <pin or -1 if via IO expander>
#define TOUCH_INT_PIN   <pin>

// LVGL buffer size
#define DRAW_BUF_SIZE   (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
#define USE_PSRAM_BUFFER <0 or 1>

// DPI for this display
#define DISPLAY_DPI     <dpi>

#endif
```

### 2. Register the Board in boards.h

Edit `saildrop/boards/boards.h` to include your board:

```c
#if defined(BOARD_<NAME>)
    #include "board_<name>.h"
#elif defined(BOARD_LCD_128)
    #include "board_lcd_128.h"
// ... other boards
#endif
```

### 3. Create LVGL Configuration (if needed)

For displays with different memory/feature requirements, create `lv_conf_<name>.h`:
- Adjust `LV_MEM_SIZE` based on available RAM
- Enable/disable `LV_USE_TFT_ESPI` based on display driver
- Configure DPI with `LV_DPI_DEF`

### 4. Add Display Driver Support (if new driver needed)

If the display uses a driver not yet supported, update `saildrop/hal.h`:
- Add include for the display library
- Add initialization code in `HAL::begin()`
- Add flush callback in `HAL::initDisplay()`

### 5. Add Touch Driver Support (if new driver needed)

If the touch controller is not yet supported:
1. Create `saildrop/drivers/<TouchDriver>.h` with the driver implementation
2. Add `#define TOUCH_DRIVER_<NAME>` option in board config
3. Update `saildrop/hal.h` to include and initialize the new driver

### 6. Handle IO Expanders (if present)

Some boards use IO expanders (PCA9557, TCA9554, etc.) for:
- Backlight control
- Display reset
- Touch reset

Add expander initialization in `HAL::initIOExpander()` if needed.


## License

This software is licensed with [Apache License 2.0](LICENSE).
