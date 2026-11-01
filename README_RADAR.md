# README_RADAR — naval radar for the Gulf of Cagliari

`radar/` is a standalone app in this repo that turns the Waveshare ESP32-S3
display into a **realtime PPI radar of the Gulf of Cagliari**: AIS boats are
plotted live relative to your house, which is pinned at **bottom center** of
the screen. No map tiles — a stylized coastline silhouette
(`radar/coastline.h`) is drawn instead.

```
                 N (compass, top-right, shows true north)
   ┌───────────────────────────────┐
   │  status: AIS 12 tgts · FEED OK │
   │        ◠  range rings          │
   │      ◠   ·  ← blip + COG vector│
   │    ◠  sweep ─╮                 │
   │   ◠       ╱                    │
   │  coastline silhouette (land)   │
   │            🏠 house (bottom    │
   │  info: NAME ...│   center)     │
   └───────────────────────────────┘
```

## Layout of this repo

| Path       | Contents                                                              |
|------------|-----------------------------------------------------------------------|
| `common/`  | header-only code **shared by both apps**: HAL, boards, touch drivers, `scale.h`, gauge `styles.h`, AIS parser `ais.h`, geo math `geo.h`, generated `board_config.h` |
| `radar/`   | the radar app: `radar.ino`, `conf.h`, `conn.h` (AIS feed client), `coastline.h`, `gauges/radargauge.h` |
| `saildrop/`| the original saildrop OS (keeps only app-specific files: screens, settings, wifi portal, its own `conn.*`) |
| `host_test/` | host-side unit test for the AIS parser + geo math (`make test`)     |
| `testdata/`| recorded NMEA/AIS traffic, `tools/serve_testdata.py` replays it      |

Sketches include shared code as `common/...` (e.g. `#include "common/hal.h"`).
The Makefile's `copy-common` target copies `common/` into the sketch before
compiling — arduino-cli compiles a *copy* of the sketch in its build dir, so
`../common` includes would not resolve. `common/` is header-only because
arduino-cli only compiles sources inside the sketch dir.

## Hardware

- **Waveshare ESP32-S3-Touch-LCD-4** (480×480, default): `make compile APP=radar BOARD=lcd4`
- Waveshare ESP32-S3-Touch-LCD-1.28 (240×240 round) also works: `BOARD=lcd128`
  (UI scales via `SCALE_PX()`; canvas buffers fall back to internal RAM)

Prereqs once per machine: `make install-prereq-lcd4` (ESP32 core + GFX lib),
LVGL 9.3 in `~/Arduino/libraries/lvgl` (the Makefile copies the matching
`lv_conf_lcd*.h` into it on every compile).

## Data feed — raw NMEA over TCP

The radar expects **raw AIS sentences (`!AIVDM`/`!AIVDO`) on a plain TCP
connection**, optionally pre-filtered to the gulf bounding box. The ESP32 side
stays identical either way; the endpoint does the shaping.

Parsed: Class A position reports (types 1/2/3), Class B (18/19), ship names
(type 5 / 24 part A). Targets outside `AIS_BBOX_*` are dropped (32 slots,
3-minute stale timeout).

### Development: replay recorded gulf traffic

`testdata/ais_29_01_2026.nmea` is a real recording from the gulf (3936
sentences, 46 distinct targets in the bbox). Serve it on any LAN machine:

```bash
python3 tools/serve_testdata.py testdata/ais_29_01_2026.nmea --tcp-port 10110 --interval 0.05
```

then point `AIS_HOST`/`AIS_PORT` at that machine.

### No feed at all: demo mode

Uncomment `#define SHOWCASE 1` in `radar/conf.h` — synthetic ferry/cargo/
sailboat targets move along realistic routes, no network needed.

### Live data

Run a small always-on bridge on a home server (or shape your tracking site)
that pulls **AISHub** (free after registration) or your own receiver and
forwards only sentences inside the gulf bbox — optionally over TLS if remote
(use `WiFiClientSecure` in `radar/conn.h`, same interface). An optional
`AIS_HELLO` line (e.g. API key) can be sent right after connecting.

## Build, flash, monitor

```bash
make compile APP=radar BOARD=lcd4   # compile
make upload                          # flash (/dev/ttyACM0)
make monitor                         # serial log (115200)
```

## Configuration (`radar/conf.h`)

| Define | Meaning |
|---|---|
| `WIFI_SSID` / `WIFI_PASSWORD` | home WiFi |
| `AIS_HOST` / `AIS_PORT` | raw-NMEA TCP endpoint |
| `AIS_HELLO` | optional line sent after connect (API key) |
| `HOME_LAT` / `HOME_LON` | house position, microdegrees (default `39215977`, `9267019` = 39.215977 N, 9.267019 E) — radar origin, drawn at bottom center |
| `RADAR_UP_BEARING` | true bearing shown at screen-up (`195` — the gulf is SSW of the house); change if your sea view faces another way |
| `AIS_BBOX_LAT/LON_MIN/MAX` | gulf filter box for incoming AIS |
| `SHOWCASE` | synthetic targets demo mode |

## Controls

- **swipe up/down**: radar range — 5 / 10 / 20 / 40 NM (default 20)
- **tap a blip**: select nearest target → name, MMSI, SOG, COG, distance and
  bearing from home (bottom-left); tap empty water to deselect
- top line: active target count + feed state (`FEED OK` / `no feed` / `DEMO`)
- top-right: compass showing where true north is on the rotated display

## How it works

- **Core 0**: `conn_loop()` (`radar/conn.h`) — WiFi reconnect, TCP feed with
  backoff, feeds `!AIVDM` chars to the shared parser (`common/ais.h`).
- **Core 1**: LVGL. `radar/gauges/radargauge.h` renders two stacked canvases:
  - *bg* (RGB565, redrawn only on range change): sea, coastline polygons
    (scanline fill), range rings centered on the house, 30° spokes, house dot,
    north compass
  - *fg* (ARGB8888 transparent, every 100 ms): sweep line (4 s/rev) with a
    60° fading phosphor trail, AIS blips (dot + COG vector; amber = Class A,
    blue = Class B; dim when stale or off-scale), selection box
- Projection (`common/geo.h`): bearing/distance from home → rotated by
  `RADAR_UP_BEARING` → screen px; scale = vertical space above the house ÷
  range, so the top of the screen is always `range` NM toward screen-up.

## Tests

```bash
make test    # g++ host test: AIS decode round-trip (types 1/18/5 multi-part),
             # bbox rejection, geo bearing/distance/destination/projection
```

The parser was additionally validated against the real recording in
`testdata/ais_29_01_2026.nmea` (46 targets decoded in the gulf bbox, names ok).

## Notes / limitations

- The coastline is a **hand-simplified silhouette**, not a chart — refine the
  point arrays in `radar/coastline.h` as you like.
- Off-scale boats are clamped to the screen edge and dimmed.
- First on-device compile of this branch has not been run in CI — if LVGL 9.3
  API details complain, they're isolated in `radargauge.h`.
