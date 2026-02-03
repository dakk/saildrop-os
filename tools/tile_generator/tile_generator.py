#!/usr/bin/env python3
"""
Saildrop-OS Tile Generator

Downloads OpenSeaMap and OpenStreetMap tiles, composites them,
and converts to RGB565 binary format for the ESP32-S3 chart display.

Usage:
    python tile_generator.py --lat 40.0 --lon 9.5 --radius 50 --zoom 10,11,12 --output /path/to/sdcard/tiles

Requirements:
    pip install Pillow requests mercantile
"""

import argparse
import os
import sys
import time
import math
from pathlib import Path

try:
    from PIL import Image
    import requests
    import mercantile
except ImportError:
    print("Missing dependencies. Install with:")
    print("  pip install Pillow requests mercantile")
    sys.exit(1)


# Tile sources
OSM_TILE_URL = "https://c.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png" 
# "https://tile.openstreetmap.org/{z}/{x}/{y}.png"
SEAMARK_TILE_URL = "https://tiles.openseamap.org/seamark/{z}/{x}/{y}.png"

# Output tile size (128x128 for faster loading on ESP32)
OUTPUT_TILE_SIZE = 128

# Request headers (OSM requires User-Agent)
HEADERS = {
    "User-Agent": "Saildrop-OS/1.0 (https://github.com/dakk/saildrop-os)"
}


def lat_lon_to_tile(lat: float, lon: float, zoom: int) -> tuple:
    """Convert lat/lon to tile coordinates."""
    tile = mercantile.tile(lon, lat, zoom)
    return tile.x, tile.y


def get_tiles_in_radius(center_lat: float, center_lon: float, radius_nm: float, zoom: int) -> list:
    """Get all tiles within radius nautical miles of center."""
    # Convert nm to degrees (approximate)
    nm_per_degree = 60.0  # 1 degree latitude ~ 60 nm
    radius_deg = radius_nm / nm_per_degree

    # Get bounding box
    min_lat = center_lat - radius_deg
    max_lat = center_lat + radius_deg
    min_lon = center_lon - radius_deg / math.cos(math.radians(center_lat))
    max_lon = center_lon + radius_deg / math.cos(math.radians(center_lat))

    # Get tile range
    min_tile = mercantile.tile(min_lon, max_lat, zoom)  # NW corner
    max_tile = mercantile.tile(max_lon, min_lat, zoom)  # SE corner

    tiles = []
    for x in range(min_tile.x, max_tile.x + 1):
        for y in range(min_tile.y, max_tile.y + 1):
            tiles.append((x, y, zoom))

    return tiles


def download_tile(url: str, max_retries: int = 3) -> Image.Image:
    """Download a tile image with retries."""
    for attempt in range(max_retries):
        try:
            response = requests.get(url, headers=HEADERS, timeout=10)
            if response.status_code == 200:
                from io import BytesIO
                return Image.open(BytesIO(response.content)).convert("RGBA")
            elif response.status_code == 404:
                return None  # Tile doesn't exist
        except Exception as e:
            if attempt < max_retries - 1:
                time.sleep(1)
            else:
                print(f"  Failed to download {url}: {e}")
    return None


def composite_tiles(base_tile: Image.Image, overlay_tile: Image.Image) -> Image.Image:
    """Composite seamark overlay onto base map tile."""
    if base_tile is None:
        return None

    result = base_tile.copy()

    if overlay_tile is not None:
        # Paste overlay with alpha channel
        result.paste(overlay_tile, (0, 0), overlay_tile)

    return result


def image_to_rgb565(img: Image.Image) -> bytes:
    """Convert PIL Image to raw RGB565 bytes (big-endian for display)."""
    # Resize to output size
    if img.size != (OUTPUT_TILE_SIZE, OUTPUT_TILE_SIZE):
        img = img.resize((OUTPUT_TILE_SIZE, OUTPUT_TILE_SIZE), Image.Resampling.LANCZOS)

    # Convert to RGB
    img = img.convert("RGB")
    pixels = img.load()

    data = bytearray(OUTPUT_TILE_SIZE * OUTPUT_TILE_SIZE * 2)

    for y in range(OUTPUT_TILE_SIZE):
        for x in range(OUTPUT_TILE_SIZE):
            r, g, b = pixels[x, y]
            # RGB565: RRRRRGGG GGGBBBBB (big-endian)
            rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            offset = (y * OUTPUT_TILE_SIZE + x) * 2
            data[offset] = (rgb565 >> 8) & 0xFF
            data[offset + 1] = rgb565 & 0xFF

    return bytes(data)


def generate_tiles(center_lat: float, center_lon: float, radius_nm: float,
                   zoom_levels: list, output_dir: str, seamark_only: bool = False):
    """Generate all tiles for the specified area."""
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    total_tiles = 0
    downloaded_tiles = 0

    for zoom in zoom_levels:
        tiles = get_tiles_in_radius(center_lat, center_lon, radius_nm, zoom)
        total_tiles += len(tiles)

        print(f"\nZoom level {zoom}: {len(tiles)} tiles")

        for i, (x, y, z) in enumerate(tiles):
            # Create directory structure
            tile_dir = output_path / str(z) / str(x)
            tile_dir.mkdir(parents=True, exist_ok=True)

            tile_file = tile_dir / f"{y}.bin"

            # Skip if already exists
            if tile_file.exists():
                print(f"  [{i+1}/{len(tiles)}] Skipping {z}/{x}/{y} (exists)")
                downloaded_tiles += 1
                continue

            print(f"  [{i+1}/{len(tiles)}] Generating {z}/{x}/{y}...", end=" ")

            # Download base map tile
            if seamark_only:
                base_tile = Image.new("RGBA", (256, 256), (26, 35, 46, 255))  # Dark water color
            else:
                base_url = OSM_TILE_URL.format(z=z, x=x, y=y)
                base_tile = download_tile(base_url)

            # Download seamark overlay
            seamark_url = SEAMARK_TILE_URL.format(z=z, x=x, y=y)
            seamark_tile = download_tile(seamark_url)

            # Composite tiles
            result = composite_tiles(base_tile, seamark_tile)

            if result is None:
                print("SKIP (no base tile)")
                continue

            # Convert to RGB565 and save
            rgb565_data = image_to_rgb565(result)

            with open(tile_file, "wb") as f:
                f.write(rgb565_data)

            downloaded_tiles += 1
            print("OK")

            # Rate limiting (be nice to tile servers)
            time.sleep(0.1)

    print(f"\n{'='*50}")
    print(f"Generated {downloaded_tiles}/{total_tiles} tiles")
    print(f"Output directory: {output_path}")
    print(f"Total size: {sum(f.stat().st_size for f in output_path.rglob('*.bin')) / 1024 / 1024:.1f} MB")


def main():
    parser = argparse.ArgumentParser(
        description="Generate chart tiles for Saildrop-OS",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Generate tiles around Sardinia (40N, 9.5E) with 50nm radius
  python tile_generator.py --lat 40.0 --lon 9.5 --radius 50 --zoom 10,11,12 --output ./tiles

  # Generate tiles with seamark overlay only (no base map)
  python tile_generator.py --lat 40.0 --lon 9.5 --radius 20 --zoom 12,13 --output ./tiles --seamark-only
        """
    )

    parser.add_argument("--lat", type=float, required=True,
                        help="Center latitude (decimal degrees)")
    parser.add_argument("--lon", type=float, required=True,
                        help="Center longitude (decimal degrees)")
    parser.add_argument("--radius", type=float, default=20,
                        help="Radius in nautical miles (default: 20)")
    parser.add_argument("--zoom", type=str, default="10,11,12",
                        help="Comma-separated zoom levels (default: 10,11,12)")
    parser.add_argument("--output", type=str, required=True,
                        help="Output directory for tiles")
    parser.add_argument("--seamark-only", action="store_true",
                        help="Only download seamark overlay, use solid water color for base")

    args = parser.parse_args()

    zoom_levels = [int(z.strip()) for z in args.zoom.split(",")]

    print(f"Saildrop-OS Tile Generator")
    print(f"{'='*50}")
    print(f"Center: {args.lat:.4f}N, {args.lon:.4f}E")
    print(f"Radius: {args.radius} nm")
    print(f"Zoom levels: {zoom_levels}")
    print(f"Output: {args.output}")
    print(f"Seamark only: {args.seamark_only}")

    generate_tiles(
        center_lat=args.lat,
        center_lon=args.lon,
        radius_nm=args.radius,
        zoom_levels=zoom_levels,
        output_dir=args.output,
        seamark_only=args.seamark_only
    )


if __name__ == "__main__":
    main()
