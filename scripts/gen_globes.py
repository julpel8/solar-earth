#!/usr/bin/env python3
"""Render orthographic globe day/night PNGs per region.

Pure Python + Pillow (no numpy). For each region we render a disc centred on
(lat0, lon0) by inverse orthographic projection and write:

  resources/images/blue_marble_<region>.png   (day:   blue ocean / green land)
  resources/images/black_marble_<region>.png  (night: dark earth + city lights)

Day comes from an equirectangular world map (land/sea), night from the DMSP
city-lights map (dark continents with warm city dots). Output is RGBA with a
transparent disc background and <=8 colours each, so the Pebble image tool
builds 4-bit palette bitmaps; the watch combines them and flips pixels
day<->night via the runtime terminator.

Usage:  python3 scripts/gen_globes.py [region]
"""
import math
import os
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DAY_SRC = os.path.join(HERE, "assets", "world_src")     # equirectangular land/sea
LIGHTS_SRC = os.path.join(HERE, "assets", "lights_src")  # equirectangular city lights
OUTDIR = os.path.join(ROOT, "resources", "images")

DISC = 150            # globe diameter in px (fits emery centre ~154x182)

# Day palette: clean two-tone (reads best at ~150px / 4-bit).
OCEAN = (20, 86, 160)
LAND = (46, 124, 50)

# Night palette: dark earth + warm city lights.
NIGHT_OCEAN = (7, 18, 40)
NIGHT_LAND = (14, 30, 18)
CITY_DIM = (170, 116, 40)
CITY_BRIGHT = (255, 224, 130)

REGIONS = {
    "europe":   (50.0,   10.0),
    "africa":   (2.0,    20.0),
    "namerica": (45.0, -100.0),
    "samerica": (-20.0, -60.0),
    "asia":     (35.0,  100.0),
    "oceania":  (-25.0, 135.0),
}


def day_color(r, g, b):
    """Two-tone day map: blue where water-ish, green otherwise."""
    return OCEAN if b >= g else LAND


def night_color(is_ocean, lr, lg, lb):
    """Dark base + city lights from the night-lights sample."""
    lum = max(lr, lg, lb)
    if lum > 120:
        return CITY_BRIGHT
    if lum > 45:
        return CITY_DIM
    return NIGHT_OCEAN if is_ocean else NIGHT_LAND


def sample(px, W, H, lon, lat):
    tx = int((lon + math.pi) / (2 * math.pi) * (W - 1))
    ty = int((math.pi / 2 - lat) / math.pi * (H - 1))
    tx = 0 if tx < 0 else (W - 1 if tx >= W else tx)
    ty = 0 if ty < 0 else (H - 1 if ty >= H else ty)
    p = px[tx, ty]
    return p[:3] if isinstance(p, tuple) else (p, p, p)


def render_region(day_tex, lights_tex, name, lat0, lon0):
    DW, DH = day_tex.size
    LW, LH = lights_tex.size
    dtex, ltex = day_tex.load(), lights_tex.load()
    R = DISC / 2.0
    cx = cy = R - 0.5
    phi1 = math.radians(lat0)
    sinp1, cosp1 = math.sin(phi1), math.cos(phi1)
    lon0r = math.radians(lon0)

    day = Image.new("RGBA", (DISC, DISC), (0, 0, 0, 0))
    night = Image.new("RGBA", (DISC, DISC), (0, 0, 0, 0))
    dpx, npx = day.load(), night.load()

    for py in range(DISC):
        y = (cy - py) / R
        for px in range(DISC):
            x = (px - cx) / R
            rho = math.hypot(x, y)
            if rho > 1.0:
                continue
            if rho < 1e-9:
                lat, lon = phi1, lon0r
            else:
                c = math.asin(rho)
                sinc, cosc = math.sin(c), math.cos(c)
                lat = math.asin(cosc * sinp1 + y * sinc * cosp1 / rho)
                lon = lon0r + math.atan2(x * sinc,
                                         rho * cosc * cosp1 - y * sinc * sinp1)
            lon = (lon + math.pi) % (2 * math.pi) - math.pi

            dr, dg, db = sample(dtex, DW, DH, lon, lat)
            dc = day_color(dr, dg, db)
            dpx[px, py] = (dc[0], dc[1], dc[2], 255)

            lr, lg, lb = sample(ltex, LW, LH, lon, lat)
            nc = night_color(dc is OCEAN, lr, lg, lb)
            npx[px, py] = (nc[0], nc[1], nc[2], 255)

    day.save(os.path.join(OUTDIR, "blue_marble_%s.png" % name))
    night.save(os.path.join(OUTDIR, "black_marble_%s.png" % name))
    print("  %-9s -> blue/black_marble_%s.png" % (name, name))


def main():
    day_tex = Image.open(DAY_SRC).convert("RGB")
    lights_tex = Image.open(LIGHTS_SRC).convert("RGB")
    os.makedirs(OUTDIR, exist_ok=True)
    only = sys.argv[1] if len(sys.argv) > 1 else None
    print("Rendering globes (disc=%dpx)" % DISC)
    for name, (lat0, lon0) in REGIONS.items():
        if only and name != only:
            continue
        render_region(day_tex, lights_tex, name, lat0, lon0)


if __name__ == "__main__":
    main()
