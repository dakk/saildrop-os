#!/usr/bin/env python3
"""Generate radar/coastline.h from GSHHG high-res + NE 10m islands.

Usage: gen_coastline_gshhg.py [gshhg.zip] [ne_10m_coastline.geojson] [output.h]
Downloads:
  https://www.soest.hawaii.edu/pwessel/gshhg/gshhg-shp-2.3.7.zip
  https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_10m_coastline.geojson

L1 = land polygons. We take the Sardinia polygon's gulf-facing subpath
(closed along the north edge) plus small island polygons fully in the box.
"""
import struct, math, sys, zipfile

ZIP = sys.argv[1] if len(sys.argv) > 1 else 'gshhg-shp-2.3.7.zip'
SHP = 'GSHHS_shp/h/GSHHS_h_L1.shp'
EPS = 0.00012            # ~13 m
LON_W = (8.75, 9.66)
LAT_MIN, LAT_MAX = 38.83, 39.36
NORTH = 39.36
AREA_LAT = (38.80, 39.40)
AREA_LON = (8.28, 9.70)

def read_shp_polygons(zf, name):
    data = zf.read(name)
    assert data[:4] == b'\x00\x00\x27\x0a', 'not a shp'
    pos = 100
    polys = []
    while pos < len(data):
        rec_no, rec_len = struct.unpack('>II', data[pos:pos+8])
        content = data[pos+8:pos+8+rec_len*2]
        stype = struct.unpack('<I', content[:4])[0]
        if stype == 0:
            pos += 8 + rec_len*2
            continue
        assert stype == 5, f'unexpected shape type {stype}'
        nparts, npoints = struct.unpack('<II', content[36:44])
        parts = struct.unpack(f'<{nparts}I', content[44:44+4*nparts])
        pts_off = 44 + 4*nparts
        pts = struct.unpack(f'<{npoints*2}d', content[pts_off:pts_off+8*npoints*2])
        rings = []
        for i, p in enumerate(parts):
            end = parts[i+1] if i+1 < nparts else npoints
            ring = [(pts[2*j+1], pts[2*j]) for j in range(p, end)]  # (lat, lon)
            rings.append(ring)
        polys.append(rings)
        pos += 8 + rec_len*2
    return polys

def rdp(points, eps):
    if len(points) < 3:
        return points
    keep = [False]*len(points)
    keep[0] = keep[-1] = True
    stack = [(0, len(points)-1)]
    while stack:
        a, b = stack.pop()
        ax, ay = points[a]; bx, by = points[b]
        dx, dy = bx-ax, by-ay
        norm = math.hypot(dx, dy)
        maxd, idx = -1.0, -1
        for i in range(a+1, b):
            px, py = points[i]
            if norm == 0:
                d = math.hypot(px-ax, py-ay)
            else:
                d = abs(dy*px - dx*py + bx*ay - by*ax)/norm
            if d > maxd:
                maxd, idx = d, i
        if maxd > eps:
            keep[idx] = True
            stack.append((a, idx)); stack.append((idx, b))
    return [p for p, k in zip(points, keep) if k]

zf = zipfile.ZipFile(ZIP)
polys = read_shp_polygons(zf, SHP)
print('polygons:', len(polys), file=sys.stderr)

# keep polygons intersecting our area
cand = []
for rings in polys:
    allpts = [p for r in rings for p in r]
    if not allpts:
        continue
    lats = [p[0] for p in allpts]; lons = [p[1] for p in allpts]
    if max(lats) >= AREA_LAT[0] and min(lats) <= AREA_LAT[1] and max(lons) >= AREA_LON[0] and min(lons) <= AREA_LON[1]:
        cand.append(rings)
print('candidates:', len(cand), file=sys.stderr)

mainland = None
for rings in cand:
    ring = rings[0]
    lats = [p[0] for p in ring]; lons = [p[1] for p in ring]
    span = (max(lats)-min(lats)) * (max(lons)-min(lons))
    if span > 0.5:   # the big Sardinia polygon
        mainland = ring

# islands from Natural Earth 10m (GSHHG 'h' lacks Sant'Antioco)
import json
ne = json.load(open(sys.argv[2] if len(sys.argv) > 2 else 'ne_10m_coastline.geojson'))
islands = []
for f in ne['features']:
    g = f['geometry']
    coords = g['coordinates'] if g['type'] == 'LineString' else None
    if not coords or coords[0] != coords[-1]:
        continue
    lats = [p[1] for p in coords]; lons = [p[0] for p in coords]
    if 38.8 < min(lats) < 39.4 and 8.2 < min(lons) < 8.6 and max(lons) < 8.6 and len(coords) > 10:
        islands.append([(lat, lon) for lon, lat in coords])

assert mainland, 'no mainland polygon'
# extract gulf subpath from raw ring
inwin = [LON_W[0] <= lon <= LON_W[1] and LAT_MIN <= lat <= LAT_MAX for lat, lon in mainland]
subs, cur = [], []
for p, ok in zip(mainland, inwin):
    if ok:
        cur.append(p)
    elif cur:
        subs.append(cur); cur = []
if cur:
    subs.append(cur)
print('mainland subpaths:', [len(s) for s in subs], file=sys.stderr)
mainland = rdp(max(subs, key=len), EPS)
poly = list(mainland) + [(NORTH, mainland[-1][1]), (NORTH, mainland[0][1])]
islands_s = [rdp(r[:-1], 0.0002) + [r[0]] for r in islands]  # simplify open, re-close
print(f'mainland={len(poly)} islands={[len(r) for r in islands_s]} total={len(poly)+sum(len(r) for r in islands_s)}', file=sys.stderr)

def emit(name, pts):
    out = [f'static const coast_point {name}[] = {{']
    for i in range(0, len(pts), 6):
        out.append('    ' + ', '.join(f'{{ {int(round(lat*1e6))}, {int(round(lon*1e6))} }}' for lat, lon in pts[i:i+6]) + ',')
    out.append('};')
    out.append(f'static const int {name}_COUNT = sizeof({name}) / sizeof({name}[0]);')
    return '\n'.join(out)

hdr = '''/*
 * coastline.h - Gulf of Cagliari coastline, generated from GSHHG high-res
 * (GSHHG_h_L1 land polygons), Douglas-Peucker simplified (~13 m).
 * Positions in microdegrees (deg * 1e6). COAST_MAIN is closed along the
 * north edge (land lies north of the coast); islands are closed rings.
 * Regenerate with tools/gen_coastline_gshhg.py.
 */
#ifndef COASTLINE_H
#define COASTLINE_H

#include <stdint.h>

struct coast_point {
    int32_t lat;  // microdegrees
    int32_t lon;  // microdegrees
};

'''
names = ['COAST_MAIN'] + [f'COAST_ISLAND_{i}' for i in range(len(islands_s))]
pts_list = [poly] + islands_s
body = ''
for name, pts in zip(names, pts_list):
    body += ('\n\n' if body else '') + emit(name, pts)
body += '\n\nstatic const coast_point* const COAST_RINGS[] = { ' + ', '.join(names) + ' };'
body += '\nstatic const int COAST_RING_LENS[] = { ' + ', '.join(f'{n}_COUNT' for n in names) + ' };'
body += f'\nstatic const int COAST_RING_COUNT = {len(names)};'

open(sys.argv[3] if len(sys.argv) > 3 else 'radar/coastline.h', 'w').write(hdr + body + '\n\n#endif // COASTLINE_H\n')
print('written radar/coastline.h', file=sys.stderr)
