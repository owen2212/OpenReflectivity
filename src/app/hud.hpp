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

#endif
