#include "app/radar_render_data.hpp"

#include <algorithm>
#include <array>
#include <numeric>

namespace {

struct ColorStop {
    float dbz;
    float r;
    float g;
    float b;
};

constexpr std::array<ColorStop, 11> kColorStops = {{
    {  5.0f, 0.20f, 0.55f, 0.80f},
    { 10.0f, 0.10f, 0.75f, 0.85f},
    { 15.0f, 0.10f, 0.85f, 0.55f},
    { 20.0f, 0.10f, 0.80f, 0.20f},
    { 25.0f, 0.30f, 0.85f, 0.10f},
    { 30.0f, 0.95f, 0.95f, 0.15f},
    { 35.0f, 0.98f, 0.75f, 0.10f},
    { 40.0f, 0.98f, 0.50f, 0.10f},
    { 50.0f, 0.95f, 0.10f, 0.10f},
    { 60.0f, 0.85f, 0.10f, 0.85f},
    { 75.0f, 0.95f, 0.95f, 0.95f},
}};

} // namespace

std::vector<unsigned char> build_dbz_lut() {
    std::vector<unsigned char> lut(static_cast<size_t>(kDbzLutSize) * 3);
    for (int i = 0; i < kDbzLutSize; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kDbzLutSize - 1);
        const float dbz = kMinDbz + t * (kMaxDbz - kMinDbz);
        size_t hi = 0;
        while (hi < kColorStops.size() && kColorStops[hi].dbz < dbz) ++hi;

        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        if (hi == 0) {
            r = kColorStops[0].r;
            g = kColorStops[0].g;
            b = kColorStops[0].b;
        } else if (hi >= kColorStops.size()) {
            const auto &s = kColorStops.back();
            r = s.r;
            g = s.g;
            b = s.b;
        } else {
            const auto &lo = kColorStops[hi - 1];
            const auto &up = kColorStops[hi];
            const float seg = (dbz - lo.dbz) / (up.dbz - lo.dbz);
            r = lo.r + seg * (up.r - lo.r);
            g = lo.g + seg * (up.g - lo.g);
            b = lo.b + seg * (up.b - lo.b);
        }

        lut[static_cast<size_t>(i) * 3 + 0] = static_cast<unsigned char>(r * 255.0f + 0.5f);
        lut[static_cast<size_t>(i) * 3 + 1] = static_cast<unsigned char>(g * 255.0f + 0.5f);
        lut[static_cast<size_t>(i) * 3 + 2] = static_cast<unsigned char>(b * 255.0f + 0.5f);
    }
    return lut;
}

ScanGpuData build_scan_gpu_data(const rsl::Scan &scan) {
    ScanGpuData out;
    const size_t radial_count = scan.radials.size();
    out.radial_count = radial_count;
    if (radial_count == 0) return out;

    size_t gate_total = 0;
    for (const rsl::Radial &radial : scan.radials) {
        gate_total += radial.gates.size();
    }
    out.gates.reserve(gate_total);

    std::vector<float> azimuths_deg;
    std::vector<float> range_bin1_per_radial;
    std::vector<float> gate_size_per_radial;
    azimuths_deg.reserve(radial_count);
    range_bin1_per_radial.reserve(radial_count);
    gate_size_per_radial.reserve(radial_count);

    for (size_t i = 0; i < radial_count; ++i) {
        const rsl::Radial &r = scan.radials[i];
        azimuths_deg.push_back(r.azimuth);
        range_bin1_per_radial.push_back(r.range_bin1);
        gate_size_per_radial.push_back(r.gate_size);

        if (!r.gates.empty()) {
            const float radial_max = r.range_bin1 + r.gate_size * static_cast<float>(r.gates.size());
            out.max_range = std::max(out.max_range, radial_max);
        } else {
            out.max_range = std::max(out.max_range, r.range_bin1);
        }

        const int radial_idx = static_cast<int>(i);
        for (size_t j = 0; j < r.gates.size(); ++j) {
            out.gates.push_back({r.gates[j], static_cast<int>(j), radial_idx});
        }
    }

    std::vector<float> delta_az_rad(radial_count, 0.0f);
    std::vector<float> az_start_rad(radial_count, 0.0f);
    std::vector<size_t> order(radial_count);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return azimuths_deg[a] < azimuths_deg[b];
    });

    constexpr float kDeg2Rad = 0.01745329252f;
    for (size_t oi = 0; oi < order.size(); ++oi) {
        const size_t idx = order[oi];
        const size_t next_idx = order[(oi + 1) % order.size()];
        const float curr = azimuths_deg[idx];
        float next = azimuths_deg[next_idx];
        if (oi + 1 == order.size()) next += 360.0f;
        float d = next - curr;
        if (d < 0.0f) d += 360.0f;
        delta_az_rad[idx] = d * kDeg2Rad;
        az_start_rad[idx] = (curr - 0.5f * d) * kDeg2Rad;
    }

    out.meta_packed.reserve(radial_count * 4);
    for (size_t i = 0; i < radial_count; ++i) {
        out.meta_packed.push_back(az_start_rad[i]);
        out.meta_packed.push_back(range_bin1_per_radial[i]);
        out.meta_packed.push_back(gate_size_per_radial[i]);
        out.meta_packed.push_back(delta_az_rad[i]);
    }
    return out;
}
