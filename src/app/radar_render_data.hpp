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

ProductRenderConfig make_product_render_config(rsl::ProductType product_type);
Texture make_product_lut_texture(rsl::ProductType product_type);
ScanGpuData build_scan_gpu_data(const rsl::Scan &scan);

#endif
