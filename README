# OpenReflectivity
OpenReflectivity is a simple OpenGL-based renderer for NEXRAD Level II data.
It uses a small wrapper around the NASA/TRMM Radar Software Library (RSL) to
decode WSR-88D volumes and renders reflectivity sweeps using instanced quads.
The initial goal is to provide a minimal, inspectable pipeline that can be
extended with better color maps, controls, and additional products.

## Overview
- Loads Level II files
- Decodes reflectivity using the vendored RSL library.
- Packs per-radial metadata into a texture buffer and draws per-gate quads.
- Vertex shader performs polar-to-Cartesian conversion; fragment shader applies
  a basic color ramp with sentinel filtering.

## Third-party

This project vendors the NASA/TRMM Radar Software Library (RSL) for decoding
WSR-88D Level II radar data.

RSL is licensed under the GNU Library General Public License (LGPL), and its
license and notices are preserved in `external/rsl/` (see `LGPL`, `GPL`, and
`Copyright`).
