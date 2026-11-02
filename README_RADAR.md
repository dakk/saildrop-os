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
   │    ◠                           │
   │   ◠                            │
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

- **swipe any direction**: radar range — 5 / 10 / 20 NM (default 20)
- **tap a blip**: select nearest target → name, MMSI, **vessel type and nav
  status** (e.g. "Cargo · Moored"), SOG, COG, distance and bearing from home,
  closest approach (bottom-left); tap empty water to deselect
- top line: active target count + feed state (`FEED OK` / `no feed` / `DEMO`)
- top-right: compass showing where true north is on the rotated display

## How it works

- **Core 0**: `conn_loop()` (`radar/conn.h`) — WiFi reconnect, TCP feed with
  backoff, feeds `!AIVDM` chars to the shared parser (`common/ais.h`).
- **Core 1**: LVGL. `radar/gauges/radargauge.h` renders two stacked canvases:
  - *bg* (RGB565, redrawn only on range change): sea, coastline polygons
    (scanline fill), range rings centered on the house, 30° spokes, house dot,
    north compass
  - single full-screen RGB565 canvas; blips drawn directly on it (restored
    from a pristine static copy on change, area-only invalidation). Blip color
    = vessel type (oceanook.com palette: cargo green, tanker red, passenger
    blue, fishing orange, sailing/pleasure purple, SAR bright red, ...);
    red override = close-pass-to-home alert; dim when stale or off-scale.
    Range rings are dark red; no sweep/spokes.
- Projection (`common/geo.h`): bearing/distance from home → rotated by
  `RADAR_UP_BEARING` → screen px; scale = vertical space above the house ÷
  range, so the top of the screen is always `range` NM toward screen-up.

## Tests

```bash
make test                              # g++ host unit test: AIS decode round-trip
                                       # (types 1/18/5 multi-part), bbox rejection,
                                       # geo bearing/distance/destination/projection/closest-approach
bash host_test/build_render_test.sh    # builds host_test/render_test: compiles the
                                       # real LVGL 9.3 + radargauge.h on the host
./host_test/render_test                # renders 1 s of frames into a framebuffer and
                                       # asserts sea/coast/rings(no-sweep)/blip pixels;
                                       # writes host_test/render.ppm
```

The parser was additionally validated against the real recording in
`testdata/ais_29_01_2026.nmea` (46 targets decoded in the gulf bbox, names ok).

## Gotchas found while bringing this up (both fixed)

- **Never call the LVGL 9.3 canvas layer API (`lv_canvas_init_layer` + `lv_draw_*`
  + `lv_canvas_finish_layer`) with many primitives outside a running dispatch
  context** — it deadlocks in `lv_draw_dispatch` and hangs `setup()` (black
  screen). `radargauge.h` therefore rasterizes directly into the canvas buffers.
- **Canvas objects are not sized from their buffer in 9.3** — always
  `lv_obj_set_size(canvas, w, h)` after `lv_canvas_set_buffer`.
- **RGB panel (lcd4) glitches (bands shifted vertically)**: the ST7701 is
  continuously scanned from its own framebuffer, and the stock
  `draw16bitRGBBitmap` flush writes pixel-by-pixel with a cache writeback each
  — a full frame takes so long that the panel shows a half-updated image.
  `HAL::rgb_flush_cb` (common/hal.h) instead memcpy's each row into the panel
  framebuffer with one `Cache_WriteBack_Addr` per row. On top of that the
  radar uses a **full-screen PSRAM draw buffer** (PARTIAL mode, one flush per
  frame, rendered in RAM — no flicker) and area-only invalidation of the blip
  layer.
- **`LV_DRAW_SW_SUPPORT_ARGB8888` must be 1** for a transparent overlay canvas
  to composite over the RGB565 display — enabled in both `lv_conf_lcd*.h`.

## Notes / limitations

- The coastline is generated from real data: GSHHG high-resolution land
  polygons for the mainland (Douglas-Peucker ~13 m) + Natural Earth 10m for
  the islands (GSHHG "h" lacks Sant'Antioco). Regenerate with
  `python3 tools/gen_coastline_gshhg.py [gshhg.zip] [ne_10m_coastline.geojson] [radar/coastline.h]`;
  hand-edit `radar/coastline.h` freely — it's only a silhouette. 3 rings,
  228 points, ~6.5 KB.
- Off-scale boats are clamped to the screen edge and dimmed.
- **Closest approach to the house** (not collision - the origin is fixed):
  blips that will pass within 0.5 NM of the house in the next 15 min are drawn
  red; the tap-info line shows `closest x.xnm in Nmin`.
- The feed client reconnects on failure and force-reconnects if the feed is
  silent for 60 s; AIS parse logs are gated behind `DEBUG`.
- Canvas drawing uses the LVGL 9.3 layer API (`lv_canvas_init_layer` +
  `lv_draw_*`), isolated in `radargauge.h`.
- Known minor: the AIS target table is written on core 0 and read on core 1
  without a lock — a blip can tear for a single frame under heavy traffic.
  Harmless visually; a seqlock can be added if it bothers you.
- The fg canvas is fully cleared and redrawn every 100 ms; on the 4" board
  that's ~0.9 MB of PSRAM traffic per frame — if the blips look jittery under
  load, raise `RADAR_FG_PERIOD_MS`.
