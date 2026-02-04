# OpenReflectivity – Agent Context

## Project summary
- OpenGL-based NEXRAD Level II renderer.
- Uses a vendored copy of NASA/TRMM RSL (Radar Software Library) for Level II decode (WSR-88D) under LGPL.
- Windowing via GLFW, GL loading via GLAD, CMake build.

## Repository layout
- `README`: short project summary + third-party notice.
- `build.sh`: convenience build script.
- `run.sh`: convenience run script.
- `examples/`: sample Level II file(s) (e.g., `KTLX20130520_000122_V06`).
- `shaders/`: GLSL sources (e.g., `ref.vert`, `ref.frag`) for instanced reflectivity rendering.
- `src/`
  - `main.cpp`: GL init + main loop. Includes a minimal RSL load example.
  - `gl/`: OpenGL helper wrappers (`buffer.*`, `shader.*`, `vertex_array.*`).
  - `rsl/`: wrapper layer you create (e.g., `rsl_wrapper.hpp/.cpp`).
- `external/`
  - `glad/`: OpenGL loader.
  - `rsl/`: vendored RSL sources (pruned to Level II ingest + core).
    - `wsr88d_decode_ar2v/`: retained helper program sources (not built in CMake).

## RSL integration details
- RSL is C; include with `extern "C" { #include "rsl.h" }` in **.cpp** files, not headers.
- RSL site DB path is overridden in CMake via:
  - `WSR88D_SITE_INFO_FILE="${CMAKE_SOURCE_DIR}/external/rsl/wsr88d_locations.dat"`
- The RSL sources are compiled into a static library target `rsl`.
- RSL warnings are suppressed only for that target.
- Wrapper (`src/rsl/rsl_wrapper.*`) converts RSL volumes into `Product -> Scan -> Radial` data with sentinel `-9999.0f` for missing gates.

## Build system (CMake)
- `app` links: `glad`, `rsl`, `glfw`, `OpenGL::GL`.
- `app` sources include `src/gl/buffer.cpp`, `src/gl/vertex_array.cpp`, and `src/gl/shader.cpp`.
- `rsl` target:
  - sources: `external/rsl/*.c` (via glob)
  - include: `external/rsl`
  - compile defs: `WSR88D_SITE_INFO_FILE=...`
  - compile options: `-w` or MSVC `/w` to suppress warnings
- Build type defaults to `Debug` if not specified.
- Requires `glfw3` and `OpenGL` packages available to CMake.

## Current behavior
- Example Level II load in `src/main.cpp` using:
  - file: `examples/KTLX20130520_000122_V06`
  - site id: `KTLX`
- If you move the file, update the path in `main.cpp`.
- `main.cpp` builds instanced gate data (`GateData`) and per-radial metadata.
- Instanced rendering uses a unit quad VBO + per-instance attributes (gate value, gate index, radial index).
- Radial metadata is packed into a `GL_TEXTURE_BUFFER` (`samplerBuffer`, `GL_RGBA32F`) with:
  - `x = azimuth_center_deg`
  - `y = range_bin1`
  - `z = gate_size`
  - `w = delta_azimuth_rad`
- Radials are sorted by azimuth to compute `delta_azimuth`; azimuth is centered (`az + 0.5*delta`) for rendering.
- Shaders:
  - `shaders/ref.vert`: polar→Cartesian conversion, azimuth-centered wedge sizing, rotates unit quad, uses `u_radial_meta`, `u_view_scale`, `u_view_offset`.
  - `shaders/ref.frag`: sentinel discard (`-9999.0f`) + simple 3‑color ramp.
- Viewport uses framebuffer size each frame (no hardcoded 800x600) and view scale is aspect-correct.

## Pruned RSL files
- Docs/examples/autotools assets removed.
- Non‑WSR‑88D format decoders removed.
- Kept WSR‑88D Level II decode path and core data model:
  - `wsr88d.c`, `wsr88d_to_radar.c`, `wsr88d_m31.c`, merge/sails helpers, `wsr88d_get_site.c`, `wsr88d_locations.dat`
  - core: `rsl.h`, `radar.c`, `volume.c`, `range.c`, `prune.c`, `sort_rays.c`, `ray_indexes.c`, `endian.c`, `gzip.c`
  - hidden dep: `uf_to_radar.c` (for `copy_sweeps_into_volume`)

## License/attribution
- RSL licenses preserved in `external/rsl/` (`LGPL`, `GPL`, `Copyright`).
- README includes credit for RSL.
