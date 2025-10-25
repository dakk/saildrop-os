# Saildrop-os

Saildrop OS is an ESP32 operating system for sailing gauges and tools.

This is a work in progress project, and currently I'm targeting the Waveshare Esp32 with 1.28 inch touch LCD, connected to an NMEA3WIFI multiplexer using wifi.

[![Watch the Short](https://img.youtube.com/vi/CF-8LODOrT0/0.jpg)](https://www.youtube.com/shorts/CF-8LODOrT0)

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


## License

This software is licensed with [Apache License 2.0](LICENSE).
