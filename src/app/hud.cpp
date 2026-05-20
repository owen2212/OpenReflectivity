#include "hud.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "app/app_state.hpp"

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

namespace {

void add_shadowed_text(ImDrawList *dl, ImVec2 pos, ImU32 color, const char *text) {
    dl->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 220), text);
    dl->AddText(pos, color, text);
}

} // namespace

CursorReadout compute_cursor_readout(const AppState &app, const ViewProjection &view,
                                     const ViewportRect &vp,
                                     const ProductRenderConfig &config) {
    CursorReadout out;
    const float sx = static_cast<float>(app.view.last_cursor_x);
    const float sy = static_cast<float>(app.view.last_cursor_y);
    if (vp.width <= 0.0f || vp.height <= 0.0f ||
        sx < vp.x || sx > vp.x + vp.width || sy < vp.y || sy > vp.y + vp.height) {
        return out;
    }
    if (view.scale_x == 0.0f || view.scale_y == 0.0f) return out;
    out.over_radar = true;

    // invert window -> NDC -> world (ndc = world * scale + offset)
    const float ndc_x = (sx - vp.x) / vp.width * 2.0f - 1.0f;
    const float ndc_y = 1.0f - (sy - vp.y) / vp.height * 2.0f;
    const float wx = (ndc_x - view.offset_x) / view.scale_x;
    const float wy = (ndc_y - view.offset_y) / view.scale_y;

    out.range_m = std::hypot(wx, wy);
    float az = std::atan2(wx, wy) * 57.29577951f;
    if (az < 0.0f) az += 360.0f;
    out.az_deg = az;
    app.projection.inverse(wx, wy, out.lat, out.lon);

    const size_t pi = product_index(app.view.current_product);
    const std::vector<rsl::Scan> &scans = app.products[pi].scans;
    if (scans.empty()) return out;
    const size_t scan_idx = static_cast<size_t>(
        std::clamp(app.view.scan_idx, 0, static_cast<int>(scans.size()) - 1));
    const rsl::Scan &scan = scans[scan_idx];

    const rsl::Radial *best = nullptr;
    float best_diff = 2.0f;  // degrees; covers super-res and legacy spacing
    for (const rsl::Radial &radial : scan.radials) {
        float diff = std::fabs(az - radial.azimuth);
        if (diff > 180.0f) diff = 360.0f - diff;
        if (diff < best_diff) {
            best_diff = diff;
            best = &radial;
        }
    }
    if (!best || best->gate_size <= 0.0f) return out;

    const float gate_f = (out.range_m - best->range_bin1) / best->gate_size;
    if (gate_f < 0.0f) return out;
    const size_t gate_idx = static_cast<size_t>(gate_f);
    if (gate_idx >= best->gates.size()) return out;

    const float value = best->gates[gate_idx];
    if (value > rsl::SENTINEL + 1.0f) {
        out.has_value = true;
        // same storm-relative correction as ref.vert, using this radial's az
        const float az_rad = best->azimuth * 0.017453292f;
        out.value = value - (std::sin(az_rad) * config.storm_u +
                             std::cos(az_rad) * config.storm_v);
    }
    return out;
}

void draw_inspector_overlay(const CursorReadout &readout, const char *unit_label,
                            const ViewportRect &vp) {
    if (!readout.over_radar) return;

    ImGui::SetNextWindowPos(ImVec2(vp.x + 12.0f, vp.y + vp.height - 12.0f),
                            ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.55f);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("##inspector", nullptr, flags)) {
        if (readout.has_value) {
            ImGui::Text("%.1f %s", readout.value, unit_label);
        } else {
            ImGui::TextDisabled("--");
        }
        ImGui::Text("%.1f km @ %03.0f deg", readout.range_m / 1000.0f, readout.az_deg);
        ImGui::Text("%.4f, %.4f", readout.lat, readout.lon);
    }
    ImGui::End();
}

void draw_ring_and_cardinal_labels(const ViewProjection &view, float max_range,
                                   const ViewportRect &vp) {
    if (vp.width <= 0.0f || vp.height <= 0.0f) return;
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    dl->PushClipRect(ImVec2(vp.x, vp.y), ImVec2(vp.x + vp.width, vp.y + vp.height), true);

    const ImU32 ring_color = IM_COL32(170, 180, 196, 200);

    // has to match OverlayRenderer's ring radii
    const float ring_km[] = {25.0f, 50.0f, 100.0f, 150.0f, 200.0f};
    char text[16];
    for (float rk : ring_km) {
        const float r_m = rk * 1000.0f;
        if (r_m > max_range * 1.05f) continue;
        // label on the northeast diagonal, away from the cardinal axes
        const float d = r_m * 0.70710678f;
        const ImVec2 pos = world_to_screen(view, d, d, vp);
        std::snprintf(text, sizeof(text), "%.0f km", rk);
        add_shadowed_text(dl, ImVec2(pos.x + 3.0f, pos.y - ImGui::GetFontSize()),
                          ring_color, text);
    }

    // cardinal letters pinned to the viewport edges along the site axes
    const ImU32 cardinal_color = IM_COL32(225, 228, 235, 230);
    const ImVec2 origin = world_to_screen(view, 0.0f, 0.0f, vp);
    const float fs = ImGui::GetFontSize();
    if (origin.x > vp.x + 8.0f && origin.x < vp.x + vp.width - 8.0f) {
        add_shadowed_text(dl, ImVec2(origin.x - fs * 0.25f, vp.y + 6.0f), cardinal_color, "N");
        add_shadowed_text(dl, ImVec2(origin.x - fs * 0.25f, vp.y + vp.height - fs - 6.0f),
                          cardinal_color, "S");
    }
    if (origin.y > vp.y + 8.0f && origin.y < vp.y + vp.height - 8.0f) {
        add_shadowed_text(dl, ImVec2(vp.x + 6.0f, origin.y - fs * 0.5f), cardinal_color, "W");
        add_shadowed_text(dl, ImVec2(vp.x + vp.width - fs * 0.75f - 6.0f, origin.y - fs * 0.5f),
                          cardinal_color, "E");
    }

    dl->PopClipRect();
}

void draw_legend_annotations(const char *product_label, const char *unit_label,
                             float min_value, float max_value, float tick_period,
                             float bar_x, float bar_y, float bar_w, float bar_h) {
    if (bar_h <= 0.0f || max_value <= min_value || tick_period <= 0.0f) return;
    ImDrawList *dl = ImGui::GetForegroundDrawList();

    char text[48];
    const ImU32 title_color = IM_COL32(230, 233, 240, 245);

    // product name with units, right-aligned above the bar
    if (unit_label && unit_label[0] != '\0') {
        std::snprintf(text, sizeof(text), "%s (%s)", product_label, unit_label);
    } else {
        std::snprintf(text, sizeof(text), "%s", product_label);
    }
    const ImVec2 title_size = ImGui::CalcTextSize(text);
    add_shadowed_text(dl, ImVec2(bar_x + bar_w - title_size.x,
                                 bar_y - title_size.y - 6.0f),
                      title_color, text);

    // numeric labels beside the ticks, left of the bar
    const ImU32 tick_color = IM_COL32(205, 210, 220, 235);
    const float first_tick = std::ceil(min_value / tick_period) * tick_period;
    for (float v = first_tick; v <= max_value + tick_period * 0.01f; v += tick_period) {
        const float frac = (v - min_value) / (max_value - min_value);
        const float y = bar_y + (1.0f - frac) * bar_h;
        if (tick_period >= 1.0f) {
            std::snprintf(text, sizeof(text), "%.0f", v);
        } else {
            std::snprintf(text, sizeof(text), "%.2f", v);
        }
        const ImVec2 size = ImGui::CalcTextSize(text);
        add_shadowed_text(dl, ImVec2(bar_x - size.x - 6.0f, y - size.y * 0.5f),
                          tick_color, text);
    }
}
