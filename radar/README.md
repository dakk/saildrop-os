# radar/

Naval radar for the Gulf of Cagliari — see **[`../README_RADAR.md`](../README_RADAR.md)**
for the full documentation (concept, data feed, build, configuration, controls).

Quick start:

```bash
make compile APP=radar BOARD=lcd4 && make upload && make monitor
# replay recorded gulf AIS traffic instead of a live feed:
python3 tools/serve_testdata.py testdata/ais_29_01_2026.nmea --tcp-port 10110 --interval 0.05
```
