# Cagliari naval radar (`radar/`)

Realtime AIS radar of the Gulf of Cagliari on the Waveshare ESP32-S3 display.
Your house (`HOME_LAT`/`HOME_LON` in `conf.h`) is pinned at **bottom center**;
the display is rotated (`RADAR_UP_BEARING 195`, the gulf is SSW of the house)
so the water fills the screen above it. Instead of a map, a stylized coastline
silhouette is drawn (`common/coastline.h`).

Shared code (HAL, boards, drivers, scale/styles, AIS parser, feed client) lives
in `common/` as header-only files and is included by sketches via `../common/...`,
so nothing is duplicated. The original saildrop app stays in `saildrop/`.

## Data

The radar expects **raw NMEA AIS (`!AIVDM`/`!AIVDO`) over plain TCP**, already
filtered to the gulf bbox (`AIS_BBOX_*` in `conf.h`) — see `AIS_HOST`/`AIS_PORT`.
Class A (types 1/2/3) and Class B (18/19) positions are parsed, plus ship names
(type 5/24).

Development without a live feed — replay the recorded gulf traffic on any
machine on the LAN:

```bash
python3 tools/serve_testdata.py testdata/ais_29_01_2026.nmea --tcp-port 10110 --interval 0.05
```

then set `AIS_HOST` to that machine. Or enable `#define SHOWCASE 1` in
`conf.h` for synthetic moving targets with no feed at all.

For live data, run a small bridge on a LAN machine that pulls AISHub (or your
own receiver) and forwards only sentences inside the bbox (optionally over TLS
if remote).

## Build / flash

```bash
make compile BOARD=lcd4    # 4" 480x480 (default for this app)
make compile BOARD=lcd128  # 1.28" round 240x240 also works
make upload
make monitor
```

## Controls

- swipe up/down: radar range (5 / 10 / 20 / 40 NM)
- tap a blip: name, MMSI, SOG, COG, distance and bearing from home
- top line: active target count + feed state; corner compass shows true north

## Tests

```bash
make test   # host-side unit test of the AIS parser + geo math (g++)
```
