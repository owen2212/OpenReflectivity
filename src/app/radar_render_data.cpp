#include "app/radar_render_data.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <numeric>

#include <glad/glad.h>

namespace {

struct ColorStop {
    float value;
    float r;
    float g;
    float b;
};

// NEXRAD-style reflectivity rainbow (5–75 dBZ).
constexpr std::array<ColorStop, 11> kDbzStops = {{
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

// NWS-style diverging velocity: inbound green, near-zero grey, outbound red.
constexpr std::array<ColorStop, 7> kVelocityStops = {{
    {-35.0f, 0.05f, 0.40f, 0.05f},
    {-20.0f, 0.10f, 0.75f, 0.10f},
    { -5.0f, 0.55f, 0.90f, 0.55f},
    {  0.0f, 0.60f, 0.60f, 0.60f},
    {  5.0f, 0.95f, 0.55f, 0.55f},
    { 20.0f, 0.85f, 0.10f, 0.10f},
    { 35.0f, 0.50f, 0.05f, 0.05f},
}};

// spectrum width: dark blue (laminar) up to red (turbulent), 0-12 m/s
constexpr std::array<ColorStop, 5> kSpectrumWidthStops = {{
    {  0.0f, 0.05f, 0.10f, 0.30f},
    {  2.0f, 0.10f, 0.55f, 0.70f},
    {  5.0f, 0.30f, 0.85f, 0.40f},
    {  8.0f, 0.95f, 0.85f, 0.20f},
    { 12.0f, 0.90f, 0.20f, 0.20f},
}};

std::vector<unsigned char> build_value_lut(const ColorStop *stops, size_t stop_count,
                                           float min_value, float max_value) {
    std::vector<unsigned char> lut(static_cast<size_t>(kLutSize) * 3);
    for (int i = 0; i < kLutSize; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kLutSize - 1);
        const float v = min_value + t * (max_value - min_value);
        size_t hi = 0;
        while (hi < stop_count && stops[hi].value < v) ++hi;

        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        if (hi == 0) {
            r = stops[0].r;
            g = stops[0].g;
            b = stops[0].b;
        } else if (hi >= stop_count) {
            const auto &s = stops[stop_count - 1];
            r = s.r;
            g = s.g;
            b = s.b;
        } else {
            const auto &lo = stops[hi - 1];
            const auto &up = stops[hi];
            const float seg = (v - lo.value) / (up.value - lo.value);
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

Texture make_lut_texture(const std::vector<unsigned char> &lut) {
    Texture texture(Texture::Target::Texture1D);
    texture.set_image_1d(GL_RGB8, kLutSize, GL_RGB, GL_UNSIGNED_BYTE, lut.data());
    texture.set_parameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    texture.set_parameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    texture.set_parameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    return texture;
}

} // namespace

ProductRenderConfig make_product_render_config(rsl::ProductType product_type) {
    ProductRenderConfig cfg;
    switch (product_type) {
        case rsl::ProductType::REFLECTIVITY:
            cfg.min_value = 5.0f;
            cfg.max_value = 75.0f;
            cfg.discard_below = 5.0f;
            cfg.unit_label = "dBZ";
            cfg.tick_period = 10.0f;
            break;
        case rsl::ProductType::VELOCITY:
            cfg.min_value = -35.0f;
            cfg.max_value = 35.0f;
            cfg.discard_below = rsl::SENTINEL + 1.0f;
            cfg.unit_label = "m/s";
            cfg.tick_period = 5.0f;
            break;
        case rsl::ProductType::SPECTRAL_WIDTH:
            cfg.min_value = 0.0f;
            cfg.max_value = 12.0f;
            cfg.discard_below = rsl::SENTINEL + 1.0f;
            cfg.unit_label = "m/s";
            cfg.tick_period = 2.0f;
            break;
    }
    return cfg;
}

Texture make_product_lut_texture(rsl::ProductType product_type) {
    const ProductRenderConfig cfg = make_product_render_config(product_type);
    std::vector<unsigned char> lut;
    switch (product_type) {
        case rsl::ProductType::REFLECTIVITY:
            lut = build_value_lut(kDbzStops.data(), kDbzStops.size(),
                                  cfg.min_value, cfg.max_value);
            break;
        case rsl::ProductType::VELOCITY:
            lut = build_value_lut(kVelocityStops.data(), kVelocityStops.size(),
                                  cfg.min_value, cfg.max_value);
            break;
        case rsl::ProductType::SPECTRAL_WIDTH:
            lut = build_value_lut(kSpectrumWidthStops.data(), kSpectrumWidthStops.size(),
                                  cfg.min_value, cfg.max_value);
            break;
    }
    return make_lut_texture(lut);
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
    const size_t N = order.size();

    // boundary_deg[oi] = wedge edge before order[oi] (midpoint to the previous
    // radial). Compute once and share between neighbors so adjacent wedges get
    // bit-exact endpoints, otherwise seams show up when zoomed in.
    // boundary[0] wraps via (last - 360), and boundary[N] = boundary[0] + 360.
    std::vector<float> boundary_deg(N + 1, 0.0f);
    for (size_t oi = 0; oi < N; ++oi) {
        const float curr = azimuths_deg[order[oi]];
        const float prev = (oi == 0)
            ? azimuths_deg[order[N - 1]] - 360.0f
            : azimuths_deg[order[oi - 1]];
        boundary_deg[oi] = 0.5f * (prev + curr);
    }
    boundary_deg[N] = boundary_deg[0] + 360.0f;

    // cap wedge half-width so a missing sector shows up as a gap instead of
    // the neighbors stretching across it
    float max_half_deg = std::numeric_limits<float>::infinity();
    if (N >= 2) {
        std::vector<float> gaps(N);
        for (size_t oi = 0; oi < N; ++oi) {
            gaps[oi] = boundary_deg[oi + 1] - boundary_deg[oi];
        }
        const size_t mid = gaps.size() / 2;
        std::nth_element(gaps.begin(), gaps.begin() + mid, gaps.end());
        const float median_gap = gaps[mid];
        if (median_gap > 0.0f) max_half_deg = 1.5f * median_gap;
    }

    for (size_t oi = 0; oi < N; ++oi) {
        const size_t idx = order[oi];
        const float curr = azimuths_deg[idx];
        float start_deg = boundary_deg[oi];
        float end_deg   = boundary_deg[oi + 1];
        start_deg = std::max(start_deg, curr - max_half_deg);
        end_deg   = std::min(end_deg,   curr + max_half_deg);
        az_start_rad[idx] = start_deg * kDeg2Rad;
        delta_az_rad[idx] = (end_deg - start_deg) * kDeg2Rad;
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
