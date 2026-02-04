#!/usr/bin/env python3
"""
Saildrop-OS Tile Generator

Downloads OpenSeaMap and OpenStreetMap tiles, composites them,
and converts to RGB565 binary format for the ESP32-S3 chart display.

Supports two rendering modes:
  - download: Fetch tiles from online tile servers (default)
  - mapnik: Render tiles locally using Mapnik with custom styles

Usage:
    # Download mode (default)
    python tile_generator.py --lat 40.0 --lon 9.5 --radius 50 --zoom 10,11,12 --output /path/to/sdcard/tiles

    # Mapnik mode with custom style
    python tile_generator.py --lat 40.0 --lon 9.5 --radius 50 --zoom 10,11,12 --output ./tiles \
        --renderer mapnik --style nautical.xml --osm-data region.osm.pbf

Requirements:
    pip install Pillow requests mercantile
    # For Mapnik mode: pip install mapnik (requires system mapnik library)
"""

import argparse
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

# Optional Mapnik support
MAPNIK_AVAILABLE = False
try:
    import mapnik
    MAPNIK_AVAILABLE = True
except ImportError:
    pass


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

# Overpass API endpoint
OVERPASS_API_URL = "https://overpass-api.de/api/interpreter"


def convert_osm_to_gpkg(osm_file: str, verbose: bool = True) -> str:
    """
    Convert OSM XML file to GeoPackage format for Mapnik compatibility.

    Args:
        osm_file: Path to .osm file
        verbose: Print progress messages

    Returns:
        Path to the .gpkg file
    """
    import subprocess
    import shutil
    import os

    osm_path = Path(osm_file)
    gpkg_path = osm_path.with_suffix('.gpkg')

    if gpkg_path.exists():
        if verbose:
            print(f"  Using existing GeoPackage: {gpkg_path}")
        return str(gpkg_path)

    # Check if ogr2ogr is available
    ogr2ogr = shutil.which('ogr2ogr')
    if not ogr2ogr:
        print("Error: ogr2ogr not found. Install GDAL:")
        print("  Gentoo: sudo emerge -av sci-libs/gdal")
        print("  Ubuntu: sudo apt install gdal-bin")
        print("  macOS: brew install gdal")
        sys.exit(1)

    if verbose:
        print(f"  Converting {osm_path.name} to GeoPackage...")

    # Find our custom osmconf.ini
    script_dir = Path(__file__).parent
    osmconf_path = script_dir / "osmconf.ini"

    # Set up environment with custom OSM config
    env = os.environ.copy()
    if osmconf_path.exists():
        env['OSM_CONFIG_FILE'] = str(osmconf_path)
        if verbose:
            print(f"  Using custom OSM config: {osmconf_path}")

    try:
        # Convert OSM to GeoPackage with all layers
        result = subprocess.run(
            [ogr2ogr, '-f', 'GPKG', str(gpkg_path), str(osm_path)],
            capture_output=True, text=True, timeout=300, env=env
        )
        if result.returncode != 0:
            print(f"Error converting OSM: {result.stderr}")
            sys.exit(1)

        if verbose:
            print(f"  Created: {gpkg_path}")

        return str(gpkg_path)

    except subprocess.TimeoutExpired:
        print("Error: Conversion timed out")
        sys.exit(1)
    except Exception as e:
        print(f"Error converting OSM: {e}")
        sys.exit(1)


def download_osm_data(center_lat: float, center_lon: float, radius_nm: float,
                      output_file: str, verbose: bool = True) -> str:
    """
    Download filtered OSM data for the specified area using Overpass API.

    Downloads only nautical-relevant features:
    - Coastlines, water bodies (lakes, rivers)
    - OpenSeaMap seamark data
    - Mountain peaks
    - Place names (cities, towns, villages, countries)
    - Major roads (for context)

    Args:
        center_lat, center_lon: Center coordinates
        radius_nm: Radius in nautical miles
        output_file: Path to save the .osm file
        verbose: Print progress messages

    Returns:
        Path to the downloaded file
    """
    # Convert nm to meters for Overpass (1 nm = 1852 m)
    radius_m = radius_nm * 1852

    if verbose:
        print(f"Downloading OSM data...")
        print(f"  Center: {center_lat:.4f}, {center_lon:.4f}")
        print(f"  Radius: {radius_nm} nm ({radius_m/1000:.1f} km)")

    # Overpass QL query for nautical-relevant features
    # Using 'around' for circular area selection
    query = f"""
[out:xml][timeout:300];
(
  // Coastlines
  way["natural"="coastline"](around:{radius_m},{center_lat},{center_lon});

  // Water bodies
  way["natural"="water"](around:{radius_m},{center_lat},{center_lon});
  relation["natural"="water"](around:{radius_m},{center_lat},{center_lon});
  way["water"](around:{radius_m},{center_lat},{center_lon});
  relation["water"](around:{radius_m},{center_lat},{center_lon});

  // Lakes
  way["natural"="lake"](around:{radius_m},{center_lat},{center_lon});
  relation["natural"="lake"](around:{radius_m},{center_lat},{center_lon});

  // Rivers and streams
  way["waterway"="river"](around:{radius_m},{center_lat},{center_lon});
  way["waterway"="stream"](around:{radius_m},{center_lat},{center_lon});
  way["waterway"="canal"](around:{radius_m},{center_lat},{center_lon});
  relation["waterway"="river"](around:{radius_m},{center_lat},{center_lon});

  // OpenSeaMap seamark data
  node["seamark:type"](around:{radius_m},{center_lat},{center_lon});
  way["seamark:type"](around:{radius_m},{center_lat},{center_lon});

  // Harbors and marinas
  way["leisure"="marina"](around:{radius_m},{center_lat},{center_lon});
  node["leisure"="marina"](around:{radius_m},{center_lat},{center_lon});
  way["harbour"](around:{radius_m},{center_lat},{center_lon});
  node["harbour"](around:{radius_m},{center_lat},{center_lon});

  // Piers and breakwaters
  way["man_made"="pier"](around:{radius_m},{center_lat},{center_lon});
  way["man_made"="breakwater"](around:{radius_m},{center_lat},{center_lon});
  way["man_made"="groyne"](around:{radius_m},{center_lat},{center_lon});

  // Mountain peaks
  node["natural"="peak"](around:{radius_m},{center_lat},{center_lon});

  // Place names
  node["place"="city"](around:{radius_m},{center_lat},{center_lon});
  node["place"="town"](around:{radius_m},{center_lat},{center_lon});
  node["place"="village"](around:{radius_m},{center_lat},{center_lon});
  node["place"="hamlet"](around:{radius_m},{center_lat},{center_lon});
  node["place"="island"](around:{radius_m},{center_lat},{center_lon});
  node["place"="islet"](around:{radius_m},{center_lat},{center_lon});
  relation["place"="island"](around:{radius_m},{center_lat},{center_lon});
  relation["admin_level"="2"](around:{radius_m},{center_lat},{center_lon});

  // Land areas (for rendering)
  relation["boundary"="administrative"]["admin_level"="2"](around:{radius_m},{center_lat},{center_lon});
  way["natural"="land"](around:{radius_m},{center_lat},{center_lon});
  relation["natural"="land"](around:{radius_m},{center_lat},{center_lon});

  // Major roads (for context on land)
  way["highway"="motorway"](around:{radius_m},{center_lat},{center_lon});
  way["highway"="trunk"](around:{radius_m},{center_lat},{center_lon});
  way["highway"="primary"](around:{radius_m},{center_lat},{center_lon});
);
out body;
>;
out skel qt;
"""

    if verbose:
        print("  Querying Overpass API (this may take a while)...")

    try:
        response = requests.post(
            OVERPASS_API_URL,
            data={"data": query},
            headers=HEADERS,
            timeout=600  # 10 minute timeout for large areas
        )
        response.raise_for_status()

        # Save to file
        output_path = Path(output_file)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_bytes(response.content)

        size_mb = len(response.content) / 1024 / 1024
        if verbose:
            print(f"  Downloaded {size_mb:.2f} MB to {output_file}")

        return str(output_path)

    except requests.exceptions.Timeout:
        print("Error: Overpass API timeout. Try a smaller radius.")
        sys.exit(1)
    except requests.exceptions.RequestException as e:
        print(f"Error downloading OSM data: {e}")
        sys.exit(1)


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
    from io import BytesIO

    for attempt in range(max_retries):
        try:
            response = requests.get(url, headers=HEADERS, timeout=10)
            if response.status_code == 200:
                # Check if response looks like an image
                content_type = response.headers.get('content-type', '')
                if 'image' not in content_type and len(response.content) < 100:
                    return None  # Likely an error response
                try:
                    return Image.open(BytesIO(response.content)).convert("RGBA")
                except Exception:
                    return None  # Invalid image data
            elif response.status_code == 404:
                return None  # Tile doesn't exist
        except Exception:
            if attempt < max_retries - 1:
                time.sleep(1)
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


class MapnikRenderer:
    """Renders map tiles using Mapnik with custom styles."""

    def __init__(self, style_file: str, osm_data: str = None, tile_size: int = 256):
        """
        Initialize Mapnik renderer.

        Args:
            style_file: Path to Mapnik XML style file
            osm_data: Path to OSM data file (.osm.pbf or .osm)
            tile_size: Size of rendered tiles (default 256)
        """
        if not MAPNIK_AVAILABLE:
            raise RuntimeError(
                "Mapnik is not installed. Install with:\n"
                "  Ubuntu/Debian: sudo apt install python3-mapnik\n"
                "  macOS: brew install mapnik && pip install mapnik\n"
                "  Or: pip install mapnik (requires libmapnik-dev)"
            )

        self.tile_size = tile_size
        self.style_file = Path(style_file)
        self._temp_style_file = None

        if not self.style_file.exists():
            raise FileNotFoundError(f"Style file not found: {style_file}")

        # If OSM data provided, preprocess style file to replace placeholder
        style_to_load = str(self.style_file)
        if osm_data:
            style_to_load = self._preprocess_style(osm_data)

        # Initialize Mapnik map
        self.map = mapnik.Map(tile_size, tile_size)
        mapnik.load_map(self.map, style_to_load)

        # Set projection to Web Mercator (EPSG:3857)
        self.map.srs = "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0.0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs +over"

    def _preprocess_style(self, osm_data: str) -> str:
        """
        Preprocess style file to replace OSMDATA placeholder with actual path.

        Returns path to processed style file (temporary if modified).
        """
        import tempfile

        osm_path = Path(osm_data).resolve()
        if not osm_path.exists():
            raise FileNotFoundError(f"OSM data file not found: {osm_data}")

        style_content = self.style_file.read_text()

        # Replace placeholder with actual path
        if 'OSMDATA' in style_content:
            style_content = style_content.replace('OSMDATA', str(osm_path))

            # Write to temporary file
            with tempfile.NamedTemporaryFile(mode='w', suffix='.xml',
                                             prefix='mapnik_style_', delete=False) as f:
                f.write(style_content)
                self._temp_style_file = f.name
            return self._temp_style_file

        return str(self.style_file)

    def __del__(self):
        """Clean up temporary style file."""
        if self._temp_style_file and Path(self._temp_style_file).exists():
            try:
                Path(self._temp_style_file).unlink()
            except Exception:
                pass

    def tile_bounds(self, x: int, y: int, z: int) -> tuple:
        """
        Get the bounding box for a tile in Web Mercator coordinates.

        Returns:
            (minx, miny, maxx, maxy) in EPSG:3857 meters
        """
        # Get tile bounds in lat/lon
        bounds = mercantile.bounds(x, y, z)

        # Convert to Web Mercator
        minx, miny = self._lonlat_to_merc(bounds.west, bounds.south)
        maxx, maxy = self._lonlat_to_merc(bounds.east, bounds.north)

        return (minx, miny, maxx, maxy)

    def _lonlat_to_merc(self, lon: float, lat: float) -> tuple:
        """Convert lon/lat to Web Mercator coordinates."""
        x = lon * 20037508.34 / 180.0
        y = math.log(math.tan((90 + lat) * math.pi / 360.0)) / (math.pi / 180.0)
        y = y * 20037508.34 / 180.0
        return (x, y)

    def render_tile(self, x: int, y: int, z: int) -> Image.Image:
        """
        Render a single tile.

        Args:
            x, y, z: Tile coordinates

        Returns:
            PIL Image in RGBA format
        """
        # Set map extent to tile bounds
        bounds = self.tile_bounds(x, y, z)
        bbox = mapnik.Box2d(bounds[0], bounds[1], bounds[2], bounds[3])
        self.map.zoom_to_box(bbox)

        # Render to image
        im = mapnik.Image(self.tile_size, self.tile_size)
        mapnik.render(self.map, im)

        # Convert to PIL Image - handle different Mapnik API versions
        from io import BytesIO

        try:
            # Try direct RGBA conversion first
            raw = im.tostring('rgba')
            pil_image = Image.frombytes('RGBA', (self.tile_size, self.tile_size), raw)
        except (RuntimeError, ValueError):
            # Fall back to PNG encoding/decoding
            png_data = im.tostring('png')
            pil_image = Image.open(BytesIO(png_data)).convert('RGBA')

        return pil_image


def create_default_nautical_style(output_path: str, osm_data: str = None,
                                   theme: str = "navionics") -> str:
    """
    Create a default nautical-themed Mapnik style file.

    Args:
        output_path: Where to save the style file
        osm_data: Path to OSM data file (optional, will be placeholder if not provided)
        theme: Color theme - "navionics" (light blue water) or "dark" (dark theme)

    Returns:
        Path to created style file
    """
    osm_file = osm_data if osm_data else "OSMDATA"

    # Navionics-style colors (light nautical chart theme)
    if theme == "navionics":
        colors = {
            "water": "#9ed4e6",           # Light blue water
            "water_shallow": "#c8eaf4",   # Very shallow water
            "land": "#f0e6d2",            # Beige/tan land
            "land_stroke": "#c4b8a0",     # Land border
            "coastline": "#2c5aa0",       # Dark blue coastline
            "river": "#9ed4e6",           # Same as water
            "road_major": "#ffffff",      # White roads
            "road_minor": "#e8e8e8",      # Light gray roads
            "building": "#d8d0c0",        # Slightly darker than land
            "label_major": "#1a3a6a",     # Dark blue labels
            "label_minor": "#4a6a9a",     # Medium blue labels
            "label_halo": "#ffffff",      # White halo
            "marina": "#b8e0f0",          # Light blue marina
            "marina_stroke": "#2c5aa0",   # Dark blue marina border
            "peak": "#8b4513",            # Brown for peaks
            "seamark": "#d4006a",         # Magenta for seamarks
        }
    else:  # dark theme
        colors = {
            "water": "#1a232e",
            "water_shallow": "#1e2832",
            "land": "#2d3a47",
            "land_stroke": "#3d4a57",
            "coastline": "#4a90a4",
            "river": "#1a232e",
            "road_major": "#4a4a4a",
            "road_minor": "#333333",
            "building": "#3d4a57",
            "label_major": "#aabbcc",
            "label_minor": "#778899",
            "label_halo": "#1a232e",
            "marina": "#1e2832",
            "marina_stroke": "#4a90a4",
            "peak": "#8899aa",
            "seamark": "#ff6699",
        }

    style_xml = f'''<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE Map []>
<Map background-color="{colors["water"]}" srs="+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0.0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs +over">

    <!-- Nautical chart style for Saildrop-OS ({theme} theme) -->

    <Style name="water">
        <Rule>
            <Filter>[natural] = 'water' or [water] != ''</Filter>
            <PolygonSymbolizer fill="{colors["water"]}" />
        </Rule>
    </Style>

    <Style name="land">
        <Rule>
            <PolygonSymbolizer fill="{colors["land"]}" />
            <LineSymbolizer stroke="{colors["land_stroke"]}" stroke-width="0.5" />
        </Rule>
    </Style>

    <Style name="coastline">
        <Rule>
            <Filter>[natural] = 'coastline'</Filter>
            <LineSymbolizer stroke="{colors["coastline"]}" stroke-width="1.5" stroke-linejoin="round" />
        </Rule>
    </Style>

    <Style name="waterway">
        <Rule>
            <Filter>[waterway] = 'river'</Filter>
            <LineSymbolizer stroke="{colors["river"]}" stroke-width="3" stroke-linejoin="round" />
        </Rule>
        <Rule>
            <Filter>[waterway] = 'canal'</Filter>
            <LineSymbolizer stroke="{colors["river"]}" stroke-width="2" stroke-linejoin="round" />
        </Rule>
        <Rule>
            <Filter>[waterway] = 'stream'</Filter>
            <LineSymbolizer stroke="{colors["river"]}" stroke-width="1" />
        </Rule>
    </Style>

    <Style name="lakes">
        <Rule>
            <Filter>[natural] = 'water' or [natural] = 'lake' or [water] = 'lake'</Filter>
            <PolygonSymbolizer fill="{colors["water"]}" />
            <LineSymbolizer stroke="{colors["coastline"]}" stroke-width="0.5" />
        </Rule>
    </Style>

    <Style name="roads">
        <Rule>
            <Filter>[highway] = 'motorway' or [highway] = 'trunk'</Filter>
            <LineSymbolizer stroke="{colors["road_major"]}" stroke-width="2.5" stroke-linejoin="round" stroke-linecap="round" />
        </Rule>
        <Rule>
            <Filter>[highway] = 'primary'</Filter>
            <LineSymbolizer stroke="{colors["road_major"]}" stroke-width="2" stroke-linejoin="round" stroke-linecap="round" />
        </Rule>
        <Rule>
            <Filter>[highway] = 'secondary' or [highway] = 'tertiary'</Filter>
            <LineSymbolizer stroke="{colors["road_minor"]}" stroke-width="1.5" stroke-linejoin="round" stroke-linecap="round" />
        </Rule>
    </Style>

    <Style name="marina">
        <Rule>
            <Filter>[leisure] = 'marina'</Filter>
            <PolygonSymbolizer fill="{colors["marina"]}" />
            <LineSymbolizer stroke="{colors["marina_stroke"]}" stroke-width="1.5" />
        </Rule>
    </Style>

    <Style name="piers">
        <Rule>
            <Filter>[man_made] = 'pier' or [man_made] = 'breakwater' or [man_made] = 'groyne'</Filter>
            <LineSymbolizer stroke="{colors["coastline"]}" stroke-width="2" />
        </Rule>
    </Style>

    <Style name="seamark">
        <Rule>
            <Filter>[seamark_type] != ''</Filter>
            <MarkersSymbolizer fill="{colors["seamark"]}" width="8" height="8" allow-overlap="true" />
        </Rule>
    </Style>

    <Style name="peaks">
        <Rule>
            <Filter>[natural] = 'peak'</Filter>
            <MarkersSymbolizer fill="{colors["peak"]}" width="6" height="6" marker-type="ellipse" />
            <TextSymbolizer face-name="DejaVu Sans Book" size="9" fill="{colors["peak"]}"
                            halo-fill="{colors["label_halo"]}" halo-radius="1" placement="point"
                            dy="-8" allow-overlap="false">
                [name]
            </TextSymbolizer>
        </Rule>
    </Style>

    <Style name="place-labels-major">
        <Rule>
            <Filter>[place] = 'city'</Filter>
            <TextSymbolizer face-name="DejaVu Sans Bold" size="14" fill="{colors["label_major"]}"
                            halo-fill="{colors["label_halo"]}" halo-radius="2" placement="point"
                            allow-overlap="false" minimum-distance="50">
                [name]
            </TextSymbolizer>
        </Rule>
        <Rule>
            <Filter>[place] = 'town'</Filter>
            <TextSymbolizer face-name="DejaVu Sans Book" size="12" fill="{colors["label_major"]}"
                            halo-fill="{colors["label_halo"]}" halo-radius="2" placement="point"
                            allow-overlap="false" minimum-distance="30">
                [name]
            </TextSymbolizer>
        </Rule>
    </Style>

    <Style name="place-labels-minor">
        <Rule>
            <Filter>[place] = 'village'</Filter>
            <TextSymbolizer face-name="DejaVu Sans Book" size="10" fill="{colors["label_minor"]}"
                            halo-fill="{colors["label_halo"]}" halo-radius="1" placement="point"
                            allow-overlap="false" minimum-distance="20">
                [name]
            </TextSymbolizer>
        </Rule>
        <Rule>
            <Filter>[place] = 'hamlet' or [place] = 'island' or [place] = 'islet'</Filter>
            <TextSymbolizer face-name="DejaVu Sans Book" size="9" fill="{colors["label_minor"]}"
                            halo-fill="{colors["label_halo"]}" halo-radius="1" placement="point"
                            allow-overlap="false" minimum-distance="15">
                [name]
            </TextSymbolizer>
        </Rule>
    </Style>

    <!-- Layers (bottom to top) -->

    <Layer name="land" srs="+proj=longlat +datum=WGS84">
        <StyleName>land</StyleName>
        <Datasource>
            <Parameter name="type">ogr</Parameter>
            <Parameter name="file">{osm_file}</Parameter>
            <Parameter name="layer">multipolygons</Parameter>
        </Datasource>
    </Layer>

    <Layer name="water" srs="+proj=longlat +datum=WGS84">
        <StyleName>water</StyleName>
        <Datasource>
            <Parameter name="type">ogr</Parameter>
            <Parameter name="file">{osm_file}</Parameter>
            <Parameter name="layer">multipolygons</Parameter>
        </Datasource>
    </Layer>

    <Layer name="lakes" srs="+proj=longlat +datum=WGS84">
        <StyleName>lakes</StyleName>
        <Datasource>
            <Parameter name="type">ogr</Parameter>
            <Parameter name="file">{osm_file}</Parameter>
            <Parameter name="layer">multipolygons</Parameter>
        </Datasource>
    </Layer>

    <Layer name="coastline" srs="+proj=longlat +datum=WGS84">
        <StyleName>coastline</StyleName>
        <Datasource>
            <Parameter name="type">ogr</Parameter>
            <Parameter name="file">{osm_file}</Parameter>
            <Parameter name="layer">lines</Parameter>
        </Datasource>
    </Layer>

    <Layer name="waterways" srs="+proj=longlat +datum=WGS84">
        <StyleName>waterway</StyleName>
        <Datasource>
            <Parameter name="type">ogr</Parameter>
            <Parameter name="file">{osm_file}</Parameter>
            <Parameter name="layer">lines</Parameter>
        </Datasource>
    </Layer>

    <Layer name="marina" srs="+proj=longlat +datum=WGS84">
        <StyleName>marina</StyleName>
        <Datasource>
            <Parameter name="type">ogr</Parameter>
            <Parameter name="file">{osm_file}</Parameter>
            <Parameter name="layer">multipolygons</Parameter>
        </Datasource>
    </Layer>

    <Layer name="piers" srs="+proj=longlat +datum=WGS84">
        <StyleName>piers</StyleName>
        <Datasource>
            <Parameter name="type">ogr</Parameter>
            <Parameter name="file">{osm_file}</Parameter>
            <Parameter name="layer">lines</Parameter>
        </Datasource>
    </Layer>

    <Layer name="roads" srs="+proj=longlat +datum=WGS84">
        <StyleName>roads</StyleName>
        <Datasource>
            <Parameter name="type">ogr</Parameter>
            <Parameter name="file">{osm_file}</Parameter>
            <Parameter name="layer">lines</Parameter>
        </Datasource>
    </Layer>

    <Layer name="seamark" srs="+proj=longlat +datum=WGS84">
        <StyleName>seamark</StyleName>
        <Datasource>
            <Parameter name="type">ogr</Parameter>
            <Parameter name="file">{osm_file}</Parameter>
            <Parameter name="layer">points</Parameter>
        </Datasource>
    </Layer>

    <Layer name="peaks" srs="+proj=longlat +datum=WGS84">
        <StyleName>peaks</StyleName>
        <Datasource>
            <Parameter name="type">ogr</Parameter>
            <Parameter name="file">{osm_file}</Parameter>
            <Parameter name="layer">points</Parameter>
        </Datasource>
    </Layer>

    <Layer name="place-labels-major" srs="+proj=longlat +datum=WGS84">
        <StyleName>place-labels-major</StyleName>
        <Datasource>
            <Parameter name="type">ogr</Parameter>
            <Parameter name="file">{osm_file}</Parameter>
            <Parameter name="layer">points</Parameter>
        </Datasource>
    </Layer>

    <Layer name="place-labels-minor" srs="+proj=longlat +datum=WGS84">
        <StyleName>place-labels-minor</StyleName>
        <Datasource>
            <Parameter name="type">ogr</Parameter>
            <Parameter name="file">{osm_file}</Parameter>
            <Parameter name="layer">points</Parameter>
        </Datasource>
    </Layer>

</Map>
'''

    output_file = Path(output_path)
    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(style_xml)
    return str(output_file)


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


def generate_tiles_download(center_lat: float, center_lon: float, radius_nm: float,
                            zoom_levels: list, output_dir: str, seamark_only: bool = False,
                            add_seamark: bool = True):
    """Generate tiles by downloading from tile servers."""
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
            if add_seamark:
                seamark_url = SEAMARK_TILE_URL.format(z=z, x=x, y=y)
                seamark_tile = download_tile(seamark_url)
            else:
                seamark_tile = None

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

    return downloaded_tiles, total_tiles


def generate_tiles_mapnik(center_lat: float, center_lon: float, radius_nm: float,
                          zoom_levels: list, output_dir: str, style_file: str,
                          osm_data: str = None, add_seamark: bool = True):
    """Generate tiles using Mapnik renderer with custom style."""
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    print(f"\nInitializing Mapnik renderer...")
    print(f"  Style file: {style_file}")

    # Convert .osm to .gpkg if needed (OGR can't read raw OSM XML directly)
    if osm_data and osm_data.endswith('.osm'):
        print(f"  OSM data: {osm_data}")
        osm_data = convert_osm_to_gpkg(osm_data)
    elif osm_data:
        print(f"  OSM data: {osm_data}")

    try:
        renderer = MapnikRenderer(style_file, osm_data)
    except Exception as e:
        print(f"Error initializing Mapnik: {e}")
        sys.exit(1)

    total_tiles = 0
    rendered_tiles = 0

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
                rendered_tiles += 1
                continue

            print(f"  [{i+1}/{len(tiles)}] Rendering {z}/{x}/{y}...", end=" ")

            try:
                # Render tile with Mapnik
                base_tile = renderer.render_tile(x, y, z)

                # Optionally add seamark overlay from OpenSeaMap
                if add_seamark:
                    seamark_url = SEAMARK_TILE_URL.format(z=z, x=x, y=y)
                    seamark_tile = download_tile(seamark_url)
                    result = composite_tiles(base_tile, seamark_tile)
                else:
                    result = base_tile

                if result is None:
                    print("SKIP (render failed)")
                    continue

                # Convert to RGB565 and save
                rgb565_data = image_to_rgb565(result)

                with open(tile_file, "wb") as f:
                    f.write(rgb565_data)

                rendered_tiles += 1
                print("OK")

            except Exception as e:
                print(f"ERROR ({e})")

    return rendered_tiles, total_tiles


def generate_tiles(center_lat: float, center_lon: float, radius_nm: float,
                   zoom_levels: list, output_dir: str, renderer: str = "download",
                   seamark_only: bool = False, style_file: str = None,
                   osm_data: str = None, add_seamark: bool = True):
    """
    Generate all tiles for the specified area.

    Args:
        center_lat, center_lon: Center coordinates
        radius_nm: Radius in nautical miles
        zoom_levels: List of zoom levels to generate
        output_dir: Output directory for tiles
        renderer: "download" or "mapnik"
        seamark_only: For download mode - use solid water color instead of base map
        style_file: For mapnik mode - path to Mapnik XML style
        osm_data: For mapnik mode - path to OSM data file
        add_seamark: Add OpenSeaMap overlay on top of base tiles
    """
    output_path = Path(output_dir)

    if renderer == "mapnik":
        if not MAPNIK_AVAILABLE:
            print("Error: Mapnik is not installed.")
            print("Install with: sudo apt install python3-mapnik")
            print("         or: pip install mapnik")
            sys.exit(1)

        if not style_file:
            # Generate default style
            default_style = output_path / "nautical_style.xml"
            print(f"No style file specified. Creating default style: {default_style}")
            style_file = create_default_nautical_style(str(default_style), osm_data)

        generated, total = generate_tiles_mapnik(
            center_lat, center_lon, radius_nm, zoom_levels,
            output_dir, style_file, osm_data, add_seamark
        )
    else:
        generated, total = generate_tiles_download(
            center_lat, center_lon, radius_nm, zoom_levels,
            output_dir, seamark_only, add_seamark
        )

    print(f"\n{'='*50}")
    print(f"Generated {generated}/{total} tiles")
    print(f"Output directory: {output_path}")
    print(f"Total size: {sum(f.stat().st_size for f in output_path.rglob('*.bin')) / 1024 / 1024:.1f} MB")


def main():
    parser = argparse.ArgumentParser(
        description="Generate chart tiles for Saildrop-OS",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Download mode (default) - fetch from online tile servers
  python tile_generator.py --lat 40.0 --lon 9.5 --radius 50 --zoom 10,11,12 --output ./tiles

  # Mapnik mode with automatic OSM data download (recommended)
  python tile_generator.py --lat 40.0 --lon 9.5 --radius 50 --zoom 10,11,12 --output ./tiles \\
      --renderer mapnik --download-data --theme navionics

  # Mapnik mode with existing OSM data file
  python tile_generator.py --lat 40.0 --lon 9.5 --radius 50 --zoom 10,11,12 --output ./tiles \\
      --renderer mapnik --osm-data sardinia.osm.pbf --theme dark

  # Generate default style file only
  python tile_generator.py --generate-style ./my_style.xml --theme navionics

  # Download OSM data only (no tile generation)
  python tile_generator.py --lat 40.0 --lon 9.5 --radius 50 --download-data --output ./data

Themes:
  - navionics: Light blue water, beige land (like Navionics charts)
  - dark: Dark theme for night use

The downloaded OSM data includes: coastlines, water bodies (lakes, rivers),
OpenSeaMap seamark data, mountain peaks, and place names.
        """
    )

    # Location arguments
    parser.add_argument("--lat", type=float,
                        help="Center latitude (decimal degrees)")
    parser.add_argument("--lon", type=float,
                        help="Center longitude (decimal degrees)")
    parser.add_argument("--radius", type=float, default=20,
                        help="Radius in nautical miles (default: 20)")
    parser.add_argument("--zoom", type=str, default="10,11,12",
                        help="Comma-separated zoom levels (default: 10,11,12)")
    parser.add_argument("--output", type=str,
                        help="Output directory for tiles")

    # Renderer selection
    parser.add_argument("--renderer", type=str, choices=["download", "mapnik"],
                        default="download",
                        help="Tile renderer: 'download' (online) or 'mapnik' (local)")

    # Download mode options
    parser.add_argument("--seamark-only", action="store_true",
                        help="[download mode] Use solid water color instead of base map")

    # Mapnik mode options
    parser.add_argument("--style", type=str,
                        help="[mapnik mode] Path to Mapnik XML style file")
    parser.add_argument("--osm-data", type=str,
                        help="[mapnik mode] Path to OSM data file (.osm, .osm.pbf)")
    parser.add_argument("--download-data", action="store_true",
                        help="Automatically download OSM data for the area via Overpass API")
    parser.add_argument("--theme", type=str, choices=["navionics", "dark"],
                        default="navionics",
                        help="Color theme for auto-generated style (default: navionics)")

    # Common options
    parser.add_argument("--no-seamark", action="store_true",
                        help="Do not add OpenSeaMap seamark overlay")

    # Utility commands
    parser.add_argument("--generate-style", type=str, metavar="FILE",
                        help="Generate a default Mapnik style file and exit")

    args = parser.parse_args()

    # Handle style generation only
    if args.generate_style:
        print(f"Generating {args.theme} nautical style: {args.generate_style}")
        style_path = create_default_nautical_style(
            args.generate_style, args.osm_data, theme=args.theme
        )
        print(f"Style file created: {style_path}")
        print("\nEdit this file to customize colors, labels, and layer visibility.")
        print("Then use with: --renderer mapnik --style " + style_path)
        return

    # Handle data download only
    if args.download_data and args.renderer == "download":
        if args.lat is None or args.lon is None:
            parser.error("--lat and --lon are required for data download")
        if args.output is None:
            parser.error("--output is required for data download")

        output_path = Path(args.output)
        output_path.mkdir(parents=True, exist_ok=True)
        osm_file = output_path / "nautical_data.osm"

        print(f"Saildrop-OS OSM Data Downloader")
        print(f"{'='*50}")
        download_osm_data(args.lat, args.lon, args.radius, str(osm_file))
        print(f"\nTo generate tiles with this data, run:")
        print(f"  python tile_generator.py --lat {args.lat} --lon {args.lon} "
              f"--radius {args.radius} --output {args.output}/tiles "
              f"--renderer mapnik --osm-data {osm_file} --theme {args.theme}")
        return

    # Validate required arguments for tile generation
    if args.lat is None or args.lon is None:
        parser.error("--lat and --lon are required for tile generation")
    if args.output is None:
        parser.error("--output is required for tile generation")

    zoom_levels = [int(z.strip()) for z in args.zoom.split(",")]

    # Handle automatic OSM data download for mapnik mode
    osm_data = args.osm_data
    if args.renderer == "mapnik" and args.download_data:
        output_path = Path(args.output)
        output_path.mkdir(parents=True, exist_ok=True)
        osm_file = output_path / "nautical_data.osm"

        if osm_file.exists():
            print(f"Using existing OSM data: {osm_file}")
        else:
            download_osm_data(args.lat, args.lon, args.radius, str(osm_file))

        osm_data = str(osm_file)

    # Convert .osm to .gpkg for Mapnik compatibility (OGR needs this)
    if args.renderer == "mapnik" and osm_data and osm_data.endswith('.osm'):
        osm_data = convert_osm_to_gpkg(osm_data)

    # Auto-generate style if using mapnik without explicit style
    style_file = args.style
    if args.renderer == "mapnik" and not style_file:
        output_path = Path(args.output)
        output_path.mkdir(parents=True, exist_ok=True)
        style_file = str(output_path / f"nautical_{args.theme}.xml")

        # Always regenerate style to ensure it has correct data path
        print(f"Generating {args.theme} style: {style_file}")
        create_default_nautical_style(style_file, osm_data, theme=args.theme)

    print(f"\nSaildrop-OS Tile Generator")
    print(f"{'='*50}")
    print(f"Center: {args.lat:.4f}N, {args.lon:.4f}E")
    print(f"Radius: {args.radius} nm")
    print(f"Zoom levels: {zoom_levels}")
    print(f"Output: {args.output}")
    print(f"Renderer: {args.renderer}")

    if args.renderer == "mapnik":
        print(f"Theme: {args.theme}")
        print(f"Style: {style_file}")
        if osm_data:
            print(f"OSM data: {osm_data}")
    else:
        print(f"Seamark only: {args.seamark_only}")

    print(f"Add seamark overlay: {not args.no_seamark}")

    generate_tiles(
        center_lat=args.lat,
        center_lon=args.lon,
        radius_nm=args.radius,
        zoom_levels=zoom_levels,
        output_dir=args.output,
        renderer=args.renderer,
        seamark_only=args.seamark_only,
        style_file=style_file,
        osm_data=osm_data,
        add_seamark=not args.no_seamark
    )


if __name__ == "__main__":
    main()
