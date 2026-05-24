#include "app/radar_render_data.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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

// classic NWS reflectivity palette, 5-75 dBZ, lerped between levels
constexpr std::array<ColorStop, 15> kDbzStops = {{
    {  5.0f, 0.016f, 0.914f, 0.906f},   // light cyan
    { 10.0f, 0.004f, 0.624f, 0.957f},   // blue
    { 15.0f, 0.012f, 0.000f, 0.957f},   // dark blue
    { 20.0f, 0.008f, 0.992f, 0.008f},   // green
    { 25.0f, 0.004f, 0.773f, 0.004f},
    { 30.0f, 0.000f, 0.557f, 0.000f},   // dark green
    { 35.0f, 0.992f, 0.973f, 0.008f},   // yellow
    { 40.0f, 0.898f, 0.737f, 0.000f},
    { 45.0f, 0.992f, 0.584f, 0.000f},   // orange
    { 50.0f, 0.992f, 0.000f, 0.000f},   // red
    { 55.0f, 0.831f, 0.000f, 0.000f},
    { 60.0f, 0.737f, 0.000f, 0.000f},   // dark red
    { 65.0f, 0.973f, 0.000f, 0.992f},   // magenta
    { 70.0f, 0.596f, 0.329f, 0.776f},   // purple
    { 75.0f, 0.992f, 0.992f, 0.992f},   // white
}};

// diverging velocity ramp, green inbound / red outbound, grey at zero.
// Stops cover the nominal +/-35 m/s range; the shader normalizes by
// min/max so a per-sweep Nyquist override rescales these for free.
constexpr std::array<ColorStop, 9> kVelocityStops = {{
    {-35.0f, 0.063f, 1.000f, 0.063f},   // bright green
    {-20.0f, 0.004f, 0.753f, 0.004f},
    {-10.0f, 0.000f, 0.533f, 0.000f},
    { -2.0f, 0.157f, 0.282f, 0.157f},   // dark green
    {  0.0f, 0.463f, 0.463f, 0.463f},   // grey
    {  2.0f, 0.282f, 0.157f, 0.157f},   // dark red
    { 10.0f, 0.627f, 0.000f, 0.000f},
    { 20.0f, 0.878f, 0.000f, 0.000f},
    { 35.0f, 1.000f, 0.251f, 0.251f},   // bright red
}};

// spectrum width: dark blue (laminar) up to red (turbulent), 0-12 m/s
constexpr std::array<ColorStop, 5> kSpectrumWidthStops = {{
    {  0.0f, 0.05f, 0.10f, 0.30f},
    {  2.0f, 0.10f, 0.55f, 0.70f},
    {  5.0f, 0.30f, 0.85f, 0.40f},
    {  8.0f, 0.95f, 0.85f, 0.20f},
    { 12.0f, 0.90f, 0.20f, 0.20f},
}};

// ZDR -4..+8 dB: greys/blues below zero, then the usual rain ramp into
// magenta/white at the big-drop extremes
constexpr std::array<ColorStop, 10> kZdrStops = {{
    { -4.0f, 0.25f, 0.25f, 0.30f},
    { -1.0f, 0.45f, 0.50f, 0.60f},
    {  0.0f, 0.75f, 0.75f, 0.78f},
    {  0.5f, 0.30f, 0.75f, 0.95f},
    {  1.5f, 0.15f, 0.75f, 0.25f},
    {  2.5f, 0.95f, 0.95f, 0.20f},
    {  4.0f, 0.98f, 0.55f, 0.10f},
    {  5.5f, 0.90f, 0.10f, 0.10f},
    {  7.0f, 0.85f, 0.15f, 0.85f},
    {  8.0f, 0.98f, 0.95f, 0.98f},
}};

// CC 0.2-1.05. Stops bunch up near 1.0 since that's the interesting band,
// which fakes a nonlinear scale through the plain linear LUT.
constexpr std::array<ColorStop, 10> kCcStops = {{
    { 0.20f, 0.10f, 0.10f, 0.35f},
    { 0.45f, 0.30f, 0.20f, 0.55f},
    { 0.65f, 0.20f, 0.45f, 0.75f},
    { 0.80f, 0.15f, 0.75f, 0.85f},
    { 0.90f, 0.20f, 0.80f, 0.30f},
    { 0.95f, 0.60f, 0.90f, 0.20f},
    { 0.97f, 0.95f, 0.90f, 0.15f},
    { 0.985f, 0.98f, 0.60f, 0.10f},
    { 1.00f, 0.95f, 0.20f, 0.10f},
    // CC routinely comes back slightly >1.0 in clean precip, keep it red
    { 1.05f, 0.65f, 0.05f, 0.10f},
}};

// PhiDP 0-360 deg, plain rainbow
constexpr std::array<ColorStop, 7> kPhiDpStops = {{
    {   0.0f, 0.25f, 0.15f, 0.60f},
    {  60.0f, 0.10f, 0.35f, 0.85f},
    { 120.0f, 0.10f, 0.80f, 0.80f},
    { 180.0f, 0.15f, 0.80f, 0.25f},
    { 240.0f, 0.95f, 0.90f, 0.15f},
    { 300.0f, 0.95f, 0.45f, 0.10f},
    { 360.0f, 0.90f, 0.10f, 0.10f},
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
        case rsl::ProductType::ZDR:
            cfg.min_value = -4.0f;
            cfg.max_value = 8.0f;
            cfg.discard_below = rsl::SENTINEL + 1.0f;
            cfg.unit_label = "dB";
            cfg.tick_period = 2.0f;
            break;
        case rsl::ProductType::CC:
            cfg.min_value = 0.2f;
            cfg.max_value = 1.05f;
            cfg.discard_below = rsl::SENTINEL + 1.0f;
            cfg.unit_label = "CC";
            cfg.tick_period = 0.1f;
            break;
        case rsl::ProductType::PHI_DP:
            cfg.min_value = 0.0f;
            cfg.max_value = 360.0f;
            cfg.discard_below = rsl::SENTINEL + 1.0f;
            cfg.unit_label = "deg";
            cfg.tick_period = 45.0f;
            break;
        case rsl::ProductType::COUNT:
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
        case rsl::ProductType::ZDR:
            lut = build_value_lut(kZdrStops.data(), kZdrStops.size(),
                                  cfg.min_value, cfg.max_value);
            break;
        case rsl::ProductType::CC:
            lut = build_value_lut(kCcStops.data(), kCcStops.size(),
                                  cfg.min_value, cfg.max_value);
            break;
        case rsl::ProductType::PHI_DP:
            lut = build_value_lut(kPhiDpStops.data(), kPhiDpStops.size(),
                                  cfg.min_value, cfg.max_value);
            break;
        case rsl::ProductType::COUNT:
            lut.assign(static_cast<size_t>(kLutSize) * 3, 0);
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

ScanPolarTexture build_scan_polar_texture(const rsl::Scan &scan) {
    ScanPolarTexture out;
    const size_t radial_count = scan.radials.size();
    if (radial_count < 2) return out;

    // needs uniform gate geometry across the sweep (true for WSR-88D within
    // a single cut), bail out otherwise and the caller falls back to wedges
    const float range_bin1 = scan.radials[0].range_bin1;
    const float gate_size = scan.radials[0].gate_size;
    if (gate_size <= 0.0f) return out;
    size_t max_gates = 0;
    for (const rsl::Radial &r : scan.radials) {
        if (std::fabs(r.range_bin1 - range_bin1) > 1.0f ||
            std::fabs(r.gate_size - gate_size) > 1.0f) {
            return out;
        }
        max_gates = std::max(max_gates, r.gates.size());
    }
    if (max_gates == 0) return out;

    std::vector<size_t> order(radial_count);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return scan.radials[a].azimuth < scan.radials[b].azimuth;
    });

    out.rows = static_cast<int>(radial_count);
    out.cols = static_cast<int>(max_gates);
    out.range_bin1 = range_bin1;
    out.gate_size = gate_size;
    out.max_range = range_bin1 + gate_size * static_cast<float>(max_gates);
    out.values.assign(radial_count * max_gates, rsl::SENTINEL);
    for (size_t row = 0; row < radial_count; ++row) {
        const rsl::Radial &r = scan.radials[order[row]];
        std::copy(r.gates.begin(), r.gates.end(),
                  out.values.begin() + static_cast<long>(row * max_gates));
    }

    // median inter-radial gap, same missing-sector rule as the wedge path
    std::vector<float> gaps(radial_count);
    for (size_t i = 0; i < radial_count; ++i) {
        const float curr = scan.radials[order[i]].azimuth;
        const float next = (i + 1 < radial_count)
            ? scan.radials[order[i + 1]].azimuth
            : scan.radials[order[0]].azimuth + 360.0f;
        gaps[i] = next - curr;
    }
    std::vector<float> sorted_gaps = gaps;
    const size_t mid = sorted_gaps.size() / 2;
    std::nth_element(sorted_gaps.begin(), sorted_gaps.begin() + mid, sorted_gaps.end());
    const float max_gap = 1.5f * std::max(sorted_gaps[mid], 1e-3f);

    // azimuth -> continuous row coordinate. Bins inside oversized gaps get -1
    // so the shader can leave them black.
    out.az_lookup.assign(kAzLookupSize, -1.0f);
    size_t seg = 0;  // segment [seg, seg+1) in sorted order
    for (int bin = 0; bin < kAzLookupSize; ++bin) {
        const float az = (static_cast<float>(bin) + 0.5f) * (360.0f / kAzLookupSize);
        // bins before the first radial belong to the wrap segment
        float start = scan.radials[order[seg]].azimuth;
        if (az < scan.radials[order[0]].azimuth) {
            const float wrap_start = scan.radials[order[radial_count - 1]].azimuth - 360.0f;
            const float gap = scan.radials[order[0]].azimuth - wrap_start;
            if (gap <= max_gap) {
                out.az_lookup[bin] = static_cast<float>(radial_count - 1) +
                                     (az - wrap_start) / gap;
            }
            continue;
        }
        while (seg + 1 < radial_count && scan.radials[order[seg + 1]].azimuth <= az) {
            ++seg;
        }
        start = scan.radials[order[seg]].azimuth;
        if (gaps[seg] <= max_gap) {
            out.az_lookup[bin] = static_cast<float>(seg) + (az - start) / gaps[seg];
        }
    }
    return out;
}
