#ifndef APP_STATE_HPP
#define APP_STATE_HPP

#include <array>
#include <cmath>
#include <optional>
#include <vector>

#include "app/geo.hpp"
#include "app/map_data.hpp"
#include "app/products.hpp"
#include "app/radar_render_data.hpp"
#include "app/view_state.hpp"
#include "gl/texture.hpp"
#include "rsl/rsl_wrapper.hpp"

constexpr float kMinPlaybackSweepsPerSecond = 0.25f;
constexpr float kMaxPlaybackSweepsPerSecond = 4.0f;

// all per-session state, handed to GLFW callbacks via the window user
// pointer and passed around the sidebar and frame loop
struct AppState {
    ViewState view;
    std::array<rsl::Product, kProductCount> products;
    std::array<std::vector<std::optional<ScanGpuData>>, kProductCount> scan_caches;
    std::array<Texture, kProductCount> luts;       // filled after GL init
    std::array<ProductRenderConfig, kProductCount> configs;
    rsl::SiteInfo site;
    bool screenshot_requested = false;

    // storm-relative velocity (velocity product only)
    bool srv_enabled = false;
    float storm_from_deg = 240.0f;  // direction the storm comes FROM
    float storm_speed_kt = 30.0f;

    // storm motion as (east, north) m/s toward the storm's heading
    void storm_motion_uv(float &u, float &v) const {
        constexpr float kKt2Mps = 0.514444f;
        constexpr float kDeg2Rad = 0.017453292f;
        const float toward = (storm_from_deg + 180.0f) * kDeg2Rad;
        const float spd = storm_speed_kt * kKt2Mps;
        u = spd * std::sin(toward);
        v = spd * std::cos(toward);
    }

    // raw lon/lat layers stay resident so a site change can re-project
    // without re-reading the asset files
    AzimuthalEquidistant projection;
    MapPolylineLayer raw_states;
    MapPolylineLayer raw_counties;
    std::vector<MapPlace> raw_places;
    std::vector<ProjectedPlace> projected_places;

    int product_scan_count(rsl::ProductType pt) const {
        return static_cast<int>(products[product_index(pt)].scans.size());
    }
};

#endif
