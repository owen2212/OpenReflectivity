# OpenReflectivity – Agent Context

## Project summary
- OpenGL-based NEXRAD Level II renderer.
- Uses a vendored copy of NASA/TRMM RSL (Radar Software Library) for Level II decode (WSR-88D) under LGPL.

## Repository layout
- `src/`
  - `main.cpp`: GL init + main loop. Includes a minimal RSL load example.
  - `rsl/`: wrapper layer you create (e.g., `rsl_wrapper.hpp/.cpp`).
- `external/`
  - `glad/`: OpenGL loader.
  - `rsl/`: vendored RSL sources (pruned to Level II ingest + core).

## RSL integration details
- RSL is C; include with `extern "C" { #include "rsl.h" }` in **.cpp** files, not headers.
- RSL site DB path is overridden in CMake via:
  - `WSR88D_SITE_INFO_FILE="${CMAKE_SOURCE_DIR}/external/rsl/wsr88d_locations.dat"`
- The RSL sources are compiled into a static library target `rsl`.
- RSL warnings are suppressed only for that target.

## Build system (CMake)
- `app` links: `glad`, `rsl`, `glfw`, `OpenGL::GL`.
- `rsl` target:
  - sources: `external/rsl/*.c` (via glob)
  - include: `external/rsl`
  - compile defs: `WSR88D_SITE_INFO_FILE=...`
  - compile options: `-w` or MSVC `/w` to suppress warnings

## Current behavior
- Example Level II load in `src/main.cpp` using:
  - file: `examples/KTLX20130520_000122_V06`
  - site id: `KTLX`
- If you move the file, update the path in `main.cpp`.

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
