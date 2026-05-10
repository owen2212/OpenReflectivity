#!/usr/bin/env python3
"""Convert map shapefiles into the binary assets under assets/maps/.

Run by hand when refreshing assets, not part of the build. See
tools/README.md for where to download the shapefiles.

Output formats (little-endian):
  .lines: "ORLN" u32 version=1, u32 polyline_count P, u32 total_vertex_count V,
          u32[P] vertices per polyline, f32[2V] interleaved lon, lat (deg WGS84)
  .pts:   "ORPT" u32 version=1, u32 count N,
          N x { f32 lon, f32 lat, u8 rank, u8 name_len, char[name_len] utf8 }
"""

import argparse
import math
import struct
import sys

try:
    import shapefile  # pyshp
except ImportError:
    sys.exit("pyshp is required: pip3 install pyshp")


def perpendicular_distance_deg(pt, a, b):
    """Approximate point-to-segment distance in degrees (lat-corrected lon)."""
    cos_lat = math.cos(math.radians(pt[1]))
    px, py = pt[0] * cos_lat, pt[1]
    ax, ay = a[0] * cos_lat, a[1]
    bx, by = b[0] * cos_lat, b[1]
    dx, dy = bx - ax, by - ay
    seg_len2 = dx * dx + dy * dy
    if seg_len2 == 0.0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / seg_len2))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def douglas_peucker(points, tol_deg):
    if len(points) < 3:
        return points
    # Iterative stack form to dodge recursion limits on long coastlines.
    keep = [False] * len(points)
    keep[0] = keep[-1] = True
    stack = [(0, len(points) - 1)]
    while stack:
        lo, hi = stack.pop()
        max_d, max_i = 0.0, -1
        for i in range(lo + 1, hi):
            d = perpendicular_distance_deg(points[i], points[lo], points[hi])
            if d > max_d:
                max_d, max_i = d, i
        if max_d > tol_deg:
            keep[max_i] = True
            stack.append((lo, max_i))
            stack.append((max_i, hi))
    return [p for p, k in zip(points, keep) if k]


def rings_from_shapefile(path):
    rings = []
    reader = shapefile.Reader(path)
    for shp in reader.iterShapes():
        if shp.shapeType == shapefile.NULL:
            continue
        parts = list(shp.parts) + [len(shp.points)]
        for i in range(len(parts) - 1):
            ring = shp.points[parts[i]:parts[i + 1]]
            if len(ring) >= 2:
                rings.append([(float(x), float(y)) for x, y in ring])
    return rings


def write_lines(path, polylines):
    total = sum(len(p) for p in polylines)
    with open(path, "wb") as f:
        f.write(b"ORLN")
        f.write(struct.pack("<III", 1, len(polylines), total))
        f.write(struct.pack("<%dI" % len(polylines), *[len(p) for p in polylines]))
        flat = []
        for p in polylines:
            for lon, lat in p:
                flat.append(lon)
                flat.append(lat)
        f.write(struct.pack("<%df" % len(flat), *flat))
    print("wrote %s: %d polylines, %d vertices (%.1f KB)" %
          (path, len(polylines), total, (16 + 4 * len(polylines) + 8 * total) / 1024.0))


def convert_lines(shp_path, out_path, tol_deg):
    rings = rings_from_shapefile(shp_path)
    out = []
    for ring in rings:
        simplified = douglas_peucker(ring, tol_deg) if tol_deg > 0 else ring
        if len(simplified) >= 2:
            out.append(simplified)
    write_lines(out_path, out)


def convert_places(shp_path, out_path):
    reader = shapefile.Reader(shp_path)
    fields = [f[0] for f in reader.fields if f[0] != "DeletionFlag"]
    name_i = fields.index("name")
    rank_i = fields.index("scalerank")
    records = []
    for sr in reader.iterShapeRecords():
        lon, lat = sr.shape.points[0]
        name = str(sr.record[name_i]).strip()
        rank = int(sr.record[rank_i])
        if not name:
            continue
        encoded = name.encode("utf-8")[:255]
        records.append((float(lon), float(lat), max(0, min(255, rank)), encoded))
    with open(out_path, "wb") as f:
        f.write(b"ORPT")
        f.write(struct.pack("<II", 1, len(records)))
        for lon, lat, rank, encoded in records:
            f.write(struct.pack("<ffBB", lon, lat, rank, len(encoded)))
            f.write(encoded)
    print("wrote %s: %d places" % (out_path, len(records)))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--states")
    ap.add_argument("--counties")
    ap.add_argument("--places")
    ap.add_argument("--out", default="assets/maps")
    ap.add_argument("--county-tol-deg", type=float, default=0.003,
                    help="Douglas-Peucker tolerance for counties (~300 m)")
    args = ap.parse_args()

    import os
    os.makedirs(args.out, exist_ok=True)
    if args.states:
        convert_lines(args.states, os.path.join(args.out, "states.lines"), 0.0)
    if args.counties:
        convert_lines(args.counties, os.path.join(args.out, "counties.lines"),
                      args.county_tol_deg)
    if args.places:
        convert_places(args.places, os.path.join(args.out, "places.pts"))


if __name__ == "__main__":
    main()
