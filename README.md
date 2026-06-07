# OpenReflectivity

OpenReflectivity is an OpenGL-based NEXRAD Level II radar viewer. It decodes
WSR-88D archives with a small wrapper around the NASA/TRMM Radar Software
Library (RSL) and renders them in a GR2Analyst-like style: georeferenced
moment data over state/county map overlays, with dual-pol products, smoothing,
and time animation.

## Features

- Six radar moments: reflectivity, velocity, spectrum width, ZDR, correlation
  coefficient, and differential phase (dual-pol needs Build 12+ archives).
  Each has an NWS-style colormap and a labeled legend.
- State and county boundaries plus city labels, projected
  azimuthal-equidistant around the radar site. Assets are bundled and can be
  regenerated with `tools/build_map_assets.py`.
- Crisp per-gate polar quads by default, or a GR2-style smoothed rendering
  path (toggle in the sidebar).
- Cursor readout: gate value, range/azimuth, lat/lon under the mouse.
- Storm-relative velocity with a sidebar storm motion vector.
- Time animation: load a directory of volumes and play the same elevation
  across time. Decoding happens on a background thread with an LRU cache.
- Drag and drop any Level II archive (or a directory) onto the window.
- `P` saves a PNG screenshot to `screenshots/`.

## Dependencies

- `cmake`
- `GLFW3`
- OpenGL 3.3+
- `BZIP2`

## Building and running

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/app [level2-file-or-directory] [site-id] [product]
# e.g.
./build/app examples/KTLX20130520_000122_V06 KTLX reflectivity
```

Controls: `[` and `]` change elevation, `,` and `.` step volumes, `1`-`6`
switch product, drag to pan, scroll to zoom, `P` screenshot, `R` reset view,
`Esc` quit. The sidebar handles products, tilt selection, playback (sweeps or
time) with speed control, smoothing, and storm-relative velocity.

Sample volumes can be pulled from the public AWS bucket
(`unidata-nexrad-level2`), e.g.
`https://unidata-nexrad-level2.s3.amazonaws.com/2013/05/20/KTLX/KTLX20130520_000122_V06.gz`
(gunzip before loading).

## Third-party

- NASA/TRMM Radar Software Library (RSL) for WSR-88D decoding, vendored in
  `external/rsl/`. RSL is LGPL; its license and notices are preserved there.
- Dear ImGui (`external/imgui/`), glad (`external/glad/`), stb_image_write
  (`external/stb/`).
- Map data derives from US Census cartographic boundary files and Natural
  Earth (both public domain), see `tools/README.md`.

## Acknowledgements

This project took inspiration and was partially based off of
- [nexrad-level-2-data](https://github.com/netbymatt/nexrad-level-2-data)
- [Learn OpenGL](https://learnopengl.com/)
