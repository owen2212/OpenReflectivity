#ifndef RADAR_RENDER_DATA_HPP
#define RADAR_RENDER_DATA_HPP

#include <cstddef>
#include <vector>

#include "gl/texture.hpp"
#include "rsl/rsl_wrapper.hpp"

constexpr int kLutSize = 256;

struct ProductRenderConfig {
    float min_value = 0.0f;
    float max_value = 1.0f;
    float discard_below = -9990.0f;
    const char *unit_label = "";
    float tick_period = 1.0f;
    // storm motion (east, north) m/s subtracted per gate along the radial.
    // Only nonzero for velocity in storm-relative mode. ref.vert and the
    // cursor inspector both need to apply the same pair.
    float storm_u = 0.0f;
    float storm_v = 0.0f;
};

struct GateData {
    float gate;
    int gate_idx;
    int radial_idx;
};

struct ScanGpuData {
    std::vector<GateData> gates;
    std::vector<float> meta_packed;
    float max_range = 0.0f;
    size_t radial_count = 0;
};

// sweep packed as a 2D polar grid for the smoothed path: rows are radials
// sorted by azimuth, cols are gates (sentinel-padded). az_lookup maps
// azimuth in 0.1 deg bins to a continuous row coord, -1 in gaps wider than
// 1.5x the median spacing so missing sectors stay black.
constexpr int kAzLookupSize = 3600;

struct ScanPolarTexture {
    std::vector<float> values;      // rows * cols, row-major
    int cols = 0;
    int rows = 0;
    std::vector<float> az_lookup;   // kAzLookupSize entries
    float range_bin1 = 0.0f;
    float gate_size = 0.0f;
    float max_range = 0.0f;

    bool valid() const { return rows > 0 && cols > 0; }
};

ProductRenderConfig make_product_render_config(rsl::ProductType product_type);
Texture make_product_lut_texture(rsl::ProductType product_type);
ScanGpuData build_scan_gpu_data(const rsl::Scan &scan);

// returns an empty result if the sweep's radials disagree on gate geometry,
// callers fall back to the crisp instanced path
ScanPolarTexture build_scan_polar_texture(const rsl::Scan &scan);

#endif
