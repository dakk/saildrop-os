# Saildrop Chart Stream Plugin for OpenCPN

This plugin captures the current chart view from OpenCPN and streams it to a Saildrop ESP32 device via TCP.

## Features

- Captures chart view centered on boat position
- Streams JPEG-compressed images at 1 FPS
- Configurable target resolution and JPEG quality
- Zoom control from ESP32 via swipe gestures
- Auto-reconnect on connection loss

## Requirements

- OpenCPN 5.0 or later
- wxWidgets 3.0 or later
- CMake 3.10 or later
- C++14 compatible compiler

## Building

```bash
cd opencpn_plugin
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
```

### Installing

After building, the plugin can be installed to the OpenCPN plugins directory:

```bash
make install
```

Or manually copy the built library:

- **Linux**: `~/.local/lib/opencpn/saildrop_pi.so`
- **macOS**: `~/Library/Application Support/OpenCPN/Contents/PlugIns/saildrop_pi.dylib`
- **Windows**: `%APPDATA%/OpenCPN/plugins/saildrop_pi.dll`

## Configuration

1. Open OpenCPN
2. Go to Options > Plugins
3. Enable "Saildrop Chart Stream"
4. Click the Preferences button to configure:
   - **ESP32 IP Address**: The IP address of your Saildrop device (default: 192.168.4.1)
   - **Screen Width/Height**: Target resolution (default: 240x240)
   - **JPEG Quality**: Compression quality 30-95% (default: 70%)

## Usage

1. Ensure your Saildrop ESP32 device is powered on and connected to the same network
2. Click the Saildrop toolbar button to start streaming
3. The chart view will be sent to the ESP32 at 1 frame per second
4. Swipe up/down on the ESP32 to zoom in/out
5. Click the toolbar button again to stop streaming

## Protocol

The plugin communicates with the ESP32 over TCP port 2002 using a simple binary protocol:

### Packet Format
```
[MAGIC:1][TYPE:1][LEN:2 big-endian][PAYLOAD:LEN bytes]
MAGIC = 0xCA
```

### Message Types

| Type | Direction | Name | Payload |
|------|-----------|------|---------|
| 0x01 | Plugin→ESP | CHART_IMAGE | seq(2) + zoom(1) + jpeg_data |
| 0x02 | Plugin→ESP | CONFIG | width(2) + height(2) |
| 0x81 | ESP→Plugin | ZOOM_CMD | direction(1): 1=in, 2=out |
| 0x82 | ESP→Plugin | REFRESH | (empty) |
| 0x83 | ESP→Plugin | HEARTBEAT | (empty) |

## License

Apache License 2.0

## Author

Davide Gessa
