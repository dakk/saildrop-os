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

# OSM Land polygons (pre-processed coastlines)
LAND_POLYGONS_URL = "https://osmdata.openstreetmap.de/download/simplified-land-polygons-complete-3857.zip"


def download_land_polygons(output_dir: str, verbose: bool = True) -> str:
    """
    Download pre-processed OSM land polygons shapefile.

    These are coastlines converted to proper land polygons by OpenStreetMap.
    Required for proper land/water rendering since raw OSM coastlines are just lines.

    Args:
        output_dir: Directory to save the shapefile
        verbose: Print progress messages

    Returns:
        Path to the shapefile
    """
    import zipfile

    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    shapefile_dir = output_path / "simplified-land-polygons-complete-3857"
    shapefile_path = shapefile_dir / "simplified_land_polygons.shp"

    if shapefile_path.exists():
        if verbose:
            print(f"  Using existing land polygons: {shapefile_path}")
        return str(shapefile_path)

    zip_path = output_path / "land-polygons.zip"

    if verbose:
        print("Downloading OSM land polygons (this may take a while)...")
        print(f"  URL: {LAND_POLYGONS_URL}")

    try:
        response = requests.get(LAND_POLYGONS_URL, headers=HEADERS, stream=True, timeout=600)
        response.raise_for_status()

        total_size = int(response.headers.get('content-length', 0))
        downloaded = 0

        with open(zip_path, 'wb') as f:
            for chunk in response.iter_content(chunk_size=8192):
                f.write(chunk)
                downloaded += len(chunk)
                if verbose and total_size > 0:
                    pct = downloaded * 100 // total_size
                    print(f"\r  Downloading: {pct}% ({downloaded // 1024 // 1024}MB)", end="")

        if verbose:
            print(f"\n  Downloaded: {zip_path}")
            print("  Extracting...")

        with zipfile.ZipFile(zip_path, 'r') as zf:
            zf.extractall(output_path)

        # Clean up zip
        zip_path.unlink()

        if verbose:
            print(f"  Land polygons ready: {shapefile_path}")

        return str(shapefile_path)

    except Exception as e:
        print(f"Error downloading land polygons: {e}")
        print("You can manually download from: https://osmdata.openstreetmap.de/data/land-polygons.html")
        return None


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
  way["place"="island"](around:{radius_m},{center_lat},{center_lon});
  way["place"="islet"](around:{radius_m},{center_lat},{center_lon});
  relation["place"="island"](around:{radius_m},{center_lat},{center_lon});

  // Landuse for better land coverage
  way["landuse"](around:{radius_m},{center_lat},{center_lon});
  relation["landuse"](around:{radius_m},{center_lat},{center_lon});

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
                                   theme: str = "navionics", land_shapefile: str = None) -> str:
    """
    Create a default nautical-themed Mapnik style file.

    Args:
        output_path: Where to save the style file
        osm_data: Path to OSM data file (optional, will be placeholder if not provided)
        theme: Color theme - "dark", "navionics", "cm93", or "default"
        land_shapefile: Path to land polygons shapefile (for proper coastline rendering)

    Returns:
        Path to created style file
    """
    osm_file = osm_data if osm_data else "OSMDATA"
    land_file = land_shapefile if land_shapefile else osm_file

    # Color palettes based on nautical chart styles
    # Each palette: water, water_light, land, accent
    if theme == "navionics":
        colors = {
            "water": "#20B0F8",           # Bright blue water
            "water_shallow": "#A0D8F8",   # Light blue shallow
            "land": "#3E3A1C",            # Dark brown land
            "land_stroke": "#2E2A0C",     # Darker brown border
            "coastline": "#F8E870",       # Yellow coastline
            "river": "#20B0F8",           # Same as water
            "road_major": "#F8E870",      # Yellow roads
            "road_minor": "#C8B840",      # Darker yellow roads
            "building": "#4E4A2C",        # Slightly lighter than land
            "label_major": "#F8E870",     # Yellow labels
            "label_minor": "#D8C850",     # Darker yellow labels
            "label_halo": "#3E3A1C",      # Land color halo
            "marina": "#30C0F8",          # Lighter blue marina
            "marina_stroke": "#F8E870",   # Yellow marina border
            "peak": "#F8E870",            # Yellow for peaks
            "seamark": "#F8E870",         # Yellow for seamarks
        }
    elif theme == "cm93":
        colors = {
            "water": "#73B6EF",           # Medium blue water
            "water_shallow": "#D4EAEE",   # Very light blue shallow
            "land": "#525A5C",            # Gray land
            "land_stroke": "#424A4C",     # Darker gray border
            "coastline": "#C9B97A",       # Tan coastline
            "river": "#73B6EF",           # Same as water
            "road_major": "#C9B97A",      # Tan roads
            "road_minor": "#A9996A",      # Darker tan roads
            "building": "#626A6C",        # Lighter gray
            "label_major": "#C9B97A",     # Tan labels
            "label_minor": "#B9A96A",     # Darker tan labels
            "label_halo": "#525A5C",      # Land color halo
            "marina": "#83C6FF",          # Lighter blue marina
            "marina_stroke": "#C9B97A",   # Tan marina border
            "peak": "#C9B97A",            # Tan for peaks
            "seamark": "#C9B97A",         # Tan for seamarks
        }
    elif theme == "default":
        colors = {
            "water": "#6C6CA4",           # Purple-blue water
            "water_shallow": "#6C6CA4",   # Same
            "land": "#CC3333",            # Red land
            "land_stroke": "#AA2222",     # Darker red border
            "coastline": "#E7DD1D",       # Yellow coastline
            "river": "#6C6CA4",           # Same as water
            "road_major": "#E7DD1D",      # Yellow roads
            "road_minor": "#C7BD0D",      # Darker yellow roads
            "building": "#DC4343",        # Lighter red
            "label_major": "#E7DD1D",     # Yellow labels
            "label_minor": "#D7CD0D",     # Darker yellow labels
            "label_halo": "#CC3333",      # Land color halo
            "marina": "#7C7CB4",          # Lighter blue marina
            "marina_stroke": "#E7DD1D",   # Yellow marina border
            "peak": "#E7DD1D",            # Yellow for peaks
            "seamark": "#E7DD1D",         # Yellow for seamarks
        }
    else:  # dark theme (default)
        colors = {
            "water": "#16232F",           # Very dark blue water
            "water_shallow": "#070707",   # Almost black
            "land": "#363C3D",            # Dark gray land
            "land_stroke": "#464C4D",     # Lighter gray border
            "coastline": "#2C291B",       # Dark brown coastline
            "river": "#16232F",           # Same as water
            "road_major": "#565C5D",      # Medium gray roads
            "road_minor": "#464C4D",      # Darker gray roads
            "building": "#464C4D",        # Same as border
            "label_major": "#8899AA",     # Light gray labels
            "label_minor": "#667788",     # Medium gray labels
            "label_halo": "#16232F",      # Water color halo
            "marina": "#263340",          # Slightly lighter water
            "marina_stroke": "#2C291B",   # Dark brown marina border
            "peak": "#8899AA",            # Light gray for peaks
            "seamark": "#FF6699",         # Pink for seamarks
        }

    style_xml = f'''<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE Map []>
<Map background-color="{colors["water"]}" srs="+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0.0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs +over">

    <!-- Nautical chart style for Saildrop-OS ({theme} theme) -->

    <Style name="land-base">
        <Rule>
            <!-- Render land polygons with coastline border -->
            <PolygonSymbolizer fill="{colors["land"]}" />
            <LineSymbolizer stroke="{colors["coastline"]}" stroke-width="1.5" stroke-linejoin="round" />
        </Rule>
    </Style>

    <Style name="water">
        <Rule>
            <!-- Water bodies overdraw land -->
            <Filter>[natural] = 'water' or [water] != '' or [waterway] = 'riverbank'</Filter>
            <PolygonSymbolizer fill="{colors["water"]}" />
        </Rule>
    </Style>

    <Style name="coastline">
        <Rule>
            <Filter>[natural] = 'coastline'</Filter>
            <LineSymbolizer stroke="{colors["coastline"]}" stroke-width="2" stroke-linejoin="round" />
        </Rule>
    </Style>

    <Style name="land-border">
        <Rule>
            <!-- Add coastline border to islands -->
            <Filter>[place] = 'island' or [place] = 'islet'</Filter>
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

    <!-- Seamarks: Using OpenSeaMap tile overlay instead of rendering from OSM data -->
    <!-- OpenSeaMap provides proper nautical symbols (buoys, lights, etc.) -->

    <Style name="peaks">
        <Rule>
            <Filter>[natural] = 'peak'</Filter>
            <!-- Triangle marker for peaks (traditional cartographic symbol) -->
            <MarkersSymbolizer fill="{colors["peak"]}" stroke="{colors["label_halo"]}" stroke-width="0.5"
                               width="8" height="8" marker-type="arrow" transform="rotate(180)"
                               allow-overlap="false" />
            <TextSymbolizer face-name="DejaVu Sans Book" size="9" fill="{colors["peak"]}"
                            halo-fill="{colors["label_halo"]}" halo-radius="1" placement="point"
                            dy="-10" allow-overlap="false">
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

    <Layer name="land" srs="+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0.0 +k=1.0 +units=m +nadgrids=@null +wktext +no_defs +over">
        <StyleName>land-base</StyleName>
        <Datasource>
            <Parameter name="type">shape</Parameter>
            <Parameter name="file">{land_file}</Parameter>
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

    <!-- Seamarks layer removed - using OpenSeaMap tile overlay instead -->

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


def generate_preview(center_lat: float, center_lon: float, radius_nm: float,
                     zoom: int, output_file: str, renderer, tile_size: int = 256,
                     add_seamark: bool = True):
    """
    Generate a single preview PNG image showing the map area.

    Args:
        center_lat, center_lon: Center coordinates
        radius_nm: Radius in nautical miles
        zoom: Zoom level for preview
        output_file: Path to save preview PNG
        renderer: MapnikRenderer instance
        tile_size: Size of each tile
        add_seamark: Add OpenSeaMap seamark overlay
    """
    tiles = get_tiles_in_radius(center_lat, center_lon, radius_nm, zoom)
    if not tiles:
        print("No tiles in range for preview")
        return

    # Find tile bounds
    min_x = min(t[0] for t in tiles)
    max_x = max(t[0] for t in tiles)
    min_y = min(t[1] for t in tiles)
    max_y = max(t[1] for t in tiles)

    width = (max_x - min_x + 1) * tile_size
    height = (max_y - min_y + 1) * tile_size

    print(f"Generating preview: {width}x{height} pixels ({len(tiles)} tiles)")
    if add_seamark:
        print("  (with OpenSeaMap seamark overlay)")

    # Create composite image
    preview = Image.new('RGBA', (width, height), (158, 212, 230, 255))  # Water color

    for i, (x, y, z) in enumerate(tiles):
        print(f"  [{i+1}/{len(tiles)}] Rendering tile {z}/{x}/{y}...", end=" ")
        try:
            tile_img = renderer.render_tile(x, y, z)

            # Add OpenSeaMap seamark overlay
            if add_seamark:
                seamark_url = SEAMARK_TILE_URL.format(z=z, x=x, y=y)
                seamark_tile = download_tile(seamark_url)
                tile_img = composite_tiles(tile_img, seamark_tile)

            # Calculate position in composite
            px = (x - min_x) * tile_size
            py = (y - min_y) * tile_size
            preview.paste(tile_img, (px, py))
            print("OK")
        except Exception as e:
            print(f"ERROR ({e})")

    # Save preview
    preview.save(output_file, 'PNG')
    print(f"Preview saved: {output_file}")


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
    parser.add_argument("--theme", type=str, choices=["navionics", "cm93", "dark", "default"],
                        default="navionics",
                        help="Color theme for auto-generated style: navionics (bright blue/brown), cm93 (blue/gray), dark (night mode), default (purple/red)")

    # Common options
    parser.add_argument("--no-seamark", action="store_true",
                        help="Do not add OpenSeaMap seamark overlay")

    # Utility commands
    parser.add_argument("--generate-style", type=str, metavar="FILE",
                        help="Generate a default Mapnik style file and exit")
    parser.add_argument("--preview", type=str, metavar="FILE",
                        help="Generate a PNG preview image instead of tiles (mapnik mode only)")
    parser.add_argument("--preview-zoom", type=int, default=10,
                        help="Zoom level for preview image (default: 10)")

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

    # Download land polygons for mapnik mode
    land_shapefile = None
    if args.renderer == "mapnik":
        output_path = Path(args.output)
        output_path.mkdir(parents=True, exist_ok=True)
        land_shapefile = download_land_polygons(str(output_path))
        if not land_shapefile:
            print("Warning: Could not download land polygons. Land may not render correctly.")

    # Auto-generate style if using mapnik without explicit style
    style_file = args.style
    if args.renderer == "mapnik" and not style_file:
        output_path = Path(args.output)
        output_path.mkdir(parents=True, exist_ok=True)
        style_file = str(output_path / f"nautical_{args.theme}.xml")

        # Always regenerate style to ensure it has correct data path
        print(f"Generating {args.theme} style: {style_file}")
        create_default_nautical_style(style_file, osm_data, theme=args.theme, land_shapefile=land_shapefile)

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

    # Handle preview mode
    if args.preview:
        if args.renderer != "mapnik":
            print("Error: --preview requires --renderer mapnik")
            sys.exit(1)

        if not MAPNIK_AVAILABLE:
            print("Error: Mapnik is not installed for preview mode")
            sys.exit(1)

        print(f"\nGenerating preview at zoom {args.preview_zoom}...")
        try:
            renderer = MapnikRenderer(style_file, osm_data)
            generate_preview(
                args.lat, args.lon, args.radius,
                args.preview_zoom, args.preview, renderer,
                add_seamark=not args.no_seamark
            )
        except Exception as e:
            print(f"Error generating preview: {e}")
            sys.exit(1)
        return

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
