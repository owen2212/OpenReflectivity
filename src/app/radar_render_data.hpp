#ifndef RADAR_RENDER_DATA_HPP
#define RADAR_RENDER_DATA_HPP

#include <cstddef>
#include <vector>

#include "rsl/rsl_wrapper.hpp"

constexpr float kMinDbz = 5.0f;
constexpr float kMaxDbz = 75.0f;
constexpr int kDbzLutSize = 256;

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

std::vector<unsigned char> build_dbz_lut();
ScanGpuData build_scan_gpu_data(const rsl::Scan &scan);

#endif
