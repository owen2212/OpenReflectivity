#include "hud.hpp"

namespace {

uint8_t max_rank_for_zoom(float zoom) {
    if (zoom >= 8.0f) return 255;
    if (zoom >= 4.0f) return 7;
    if (zoom >= 2.0f) return 6;
    return 4;
}

} // namespace

ImVec2 world_to_screen(const ViewProjection &view, float wx, float wy,
                       const ViewportRect &vp) {
    const float ndc_x = wx * view.scale_x + view.offset_x;
    const float ndc_y = wy * view.scale_y + view.offset_y;
    return ImVec2(vp.x + (ndc_x * 0.5f + 0.5f) * vp.width,
                  vp.y + (0.5f - ndc_y * 0.5f) * vp.height);
}

void draw_place_labels(const std::vector<ProjectedPlace> &places,
                       const ViewProjection &view, float zoom,
                       const ViewportRect &vp) {
    if (places.empty() || vp.width <= 0.0f || vp.height <= 0.0f) return;

    ImDrawList *dl = ImGui::GetForegroundDrawList();
    dl->PushClipRect(ImVec2(vp.x, vp.y), ImVec2(vp.x + vp.width, vp.y + vp.height), true);

    const uint8_t max_rank = max_rank_for_zoom(zoom);
    const ImU32 dot_color = IM_COL32(235, 235, 240, 220);
    const ImU32 text_color = IM_COL32(225, 228, 235, 235);
    const ImU32 shadow_color = IM_COL32(0, 0, 0, 220);

    for (const ProjectedPlace &place : places) {
        if (place.rank > max_rank || !place.name) continue;
        const ImVec2 pos = world_to_screen(view, place.x, place.y, vp);
        if (pos.x < vp.x - 80.0f || pos.x > vp.x + vp.width + 80.0f ||
            pos.y < vp.y - 20.0f || pos.y > vp.y + vp.height + 20.0f) continue;

        dl->AddCircleFilled(pos, 2.0f, dot_color);
        const ImVec2 text_pos(pos.x + 5.0f, pos.y - ImGui::GetFontSize() * 0.5f);
        dl->AddText(ImVec2(text_pos.x + 1.0f, text_pos.y + 1.0f), shadow_color,
                    place.name->c_str());
        dl->AddText(text_pos, text_color, place.name->c_str());
    }

    dl->PopClipRect();
}
