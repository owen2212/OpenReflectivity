#ifndef HUD_HPP
#define HUD_HPP

#include <vector>

#include <imgui.h>

#include "app/map_data.hpp"
#include "app/renderers.hpp"

// radar viewport rect in window coordinates (not framebuffer pixels)
struct ViewportRect {
    float x = 0.0f;       // left edge (sidebar width)
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

ImVec2 world_to_screen(const ViewProjection &view, float wx, float wy,
                       const ViewportRect &vp);

// city labels on the ImGui foreground draw list, rank-filtered by zoom
void draw_place_labels(const std::vector<ProjectedPlace> &places,
                       const ViewProjection &view, float zoom,
                       const ViewportRect &vp);

struct AppState;

struct CursorReadout {
    bool over_radar = false;
    float range_m = 0.0f;
    float az_deg = 0.0f;
    double lat = 0.0;
    double lon = 0.0;
    bool has_value = false;
    float value = 0.0f;
};

// Resolves the gate under the cursor in the active product's current scan.
CursorReadout compute_cursor_readout(const AppState &app, const ViewProjection &view,
                                     const ViewportRect &vp);

void draw_inspector_overlay(const CursorReadout &readout, const char *unit_label,
                            const ViewportRect &vp);

// ring radii here have to match OverlayRenderer's ring set
void draw_ring_and_cardinal_labels(const ViewProjection &view, float max_range,
                                   const ViewportRect &vp);

// bar rect is in window coordinates, origin top-left
void draw_legend_annotations(const char *product_label, const char *unit_label,
                             float min_value, float max_value, float tick_period,
                             float bar_x, float bar_y, float bar_w, float bar_h);

#endif
