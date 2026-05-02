#ifndef APP_STATE_HPP
#define APP_STATE_HPP

#include <array>
#include <optional>
#include <vector>

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

    int product_scan_count(rsl::ProductType pt) const {
        return static_cast<int>(products[product_index(pt)].scans.size());
    }
};

#endif
