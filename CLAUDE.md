# Saildrop-OS - AI Summary

## Project Overview

Saildrop-OS is an embedded operating system for sailing instruments running on ESP32-S3 hardware. It displays real-time sailing data (speed, heading, wind, depth) on a 1.28" touchscreen LCD (240x240) and receives NMEA 0183 navigation data via WiFi from an NMEA multiplexer.

## Technology Stack

- **Hardware**: Waveshare ESP32-S3 with 1.28" touch LCD
- **Framework**: Arduino (arduino-cli)
- **UI Library**: LVGL v9.3.0
- **Display Driver**: TFT_eSPI
- **Touch Controller**: CST816S (capacitive, I2C)
- **NMEA Parsing**: MicroNMEA
- **Language**: C/C++ (Arduino sketch + header-only libraries)
- **Build**: Makefile with arduino-cli
- **License**: Apache 2.0

## Project Structure

```
saildrop-os/
├── saildrop/                   # Main application
│   ├── saildrop.ino           # Entry point - init, main loop, gesture handling
│   ├── conf.h                 # Configuration macros (WiFi, display, debug)
│   ├── data.h/cpp             # Global NMEA data singleton (sog, hdg, wind values)
│   ├── conn.h/cpp             # WiFi + TCP connection, NMEA parsing
│   ├── CST816S.h/cpp          # Touch controller driver
│   ├── utils.h                # Utilities (heading to cardinal conversion)
│   ├── screens/               # Screen implementations
│   │   ├── screen.h           # Base Screen & MultiScreen classes
│   │   ├── splashscreen.h     # Loading screen
│   │   ├── speedscreen.h      # Speed gauge display
│   │   ├── compassscreen.h    # Compass display
│   │   └── valuesscreen.h     # Multi-value carousel (depth, SOG, HDG)
│   └── gauges/                # Reusable gauge widgets
│       ├── speedgauge.h       # Circular speed gauge with needle
│       ├── compass.h          # 360° compass rose
│       ├── valuegauge.h       # Generic value with arc indicator
│       ├── windgauge.h        # Wind direction/speed (disabled)
│       └── timergauge.h       # Timer with controls (disabled)
├── test_wifi/                 # WiFi connectivity test sketch
├── test_lvgl9/                # LVGL v9 test sketch
├── testdata/                  # Sample NMEA data files
├── lv_conf.h                  # LVGL configuration (16-bit color, 130 DPI)
└── Makefile                   # Build automation
```

## Architecture

**Dual-Core Design:**
- **Core 0**: `conn_loop` - WiFi connection, TCP data reception, NMEA parsing
- **Core 1**: Main loop - LVGL rendering, touch input processing

**Data Flow:**
```
WiFi (NMEA Multiplexer) → TCP:2001 → MicroNMEA Parser → nmea_data singleton → LVGL Gauges → Display
```

**Screen Navigation:**
- Swipe left/right: Switch between screens (Speed, Compass, Values)
- Swipe up/down: Navigate within multi-screen carousels
- Touch gestures handled via CST816S with 100ms debounce

## Key Data Structures

```cpp
// data.h - Global navigation data
struct nmea_data {
    uint32_t sog;  // Speed over ground (0.1 knot units)
    uint32_t hdg;  // Heading (degrees)
    uint32_t tws;  // True wind speed
    uint32_t twa;  // True wind angle
    uint32_t aws;  // Apparent wind speed
    uint32_t awa;  // Apparent wind angle
};
nmea_data* get_data();  // Singleton accessor
```

## Configuration (conf.h)

Key defines:
- `DEBUG` - Enable serial debug output
- `SCREEN_WIDTH/HEIGHT` - 240x240
- `WIFI_DEFAULT_SSID/PASSWORD` - Target WiFi AP
- `WIFI_DEFAULT_IP` - NMEA multiplexer IP (default: 192.168.4.1)
- `WIFI_DEFAULT_TCP_PORT` - Data port (default: 2001)
- `AP_MODE` - Run as WiFi access point instead of station
- `SHOWCASE` - Demo mode with animated gauge values

## Build Commands

```bash
make compile    # Compile firmware
make upload     # Upload to device (/dev/ttyACM0)
make monitor    # Serial monitor (115200 baud)
make all        # compile + upload
```

## Screen/Gauge Hierarchy

```
Screen (base class)
├── SplashScreen - Loading animation during WiFi connect
├── SpeedScreen - Contains SpeedGauge (0-20 knots)
├── CompassScreen - Contains Compass (360° rose)
└── ValuesScreen (MultiScreen) - Carousel of ValueGauges
    ├── Depth gauge
    ├── SOG gauge
    └── HDG gauge
```

## Adding New Features

**New Screen:**
1. Create header in `screens/` extending `Screen` or `MultiScreen`
2. Implement `on_swipe_up/down/left/right()` handlers
3. Register in `saildrop.ino` screens array

**New Gauge:**
1. Create header in `gauges/` using LVGL widgets
2. Implement `create()` function taking parent lv_obj_t*
3. Add tick callback for data updates (100ms interval)

**New NMEA Data:**
1. Add field to `nmea_data` struct in `data.h`
2. Parse in `conn.cpp` `conn_loop()` using MicroNMEA
3. Access via `get_data()->field` in gauge tick callbacks

## Dependencies

- esp32 board package v2.0.12
- LVGL 9.3.0 (with custom lv_conf.h)
- TFT_eSPI (Waveshare package)
- MicroNMEA (bundled nmeaparser.h)
