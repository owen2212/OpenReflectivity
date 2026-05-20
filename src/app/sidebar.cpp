#include "sidebar.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "app/app_state.hpp"
#include "app/imgui_setup.hpp"

namespace {

constexpr float kSidebarWidth = 280.0f;
constexpr float kMinRadarWidth = 320.0f;
constexpr float kPlaybackSpeedWheelStep = 0.25f;

// first valid scan timestamp across products (split cuts can leave the
// current product's slot empty)
const rsl::ScanTime* find_volume_time(const AppState &app) {
    for (const rsl::Product &product : app.products) {
        for (const rsl::Scan &scan : product.scans) {
            if (scan.start_time.year > 0) return &scan.start_time;
        }
    }
    return nullptr;
}

// elevation for a scan slot, checking every product since NULL sweeps in
// the active product can still have the elevation recorded elsewhere
bool elevation_for_index(const AppState &app, size_t idx, float &out_elev) {
    for (const rsl::Product &product : app.products) {
        if (idx < product.scans.size() && !product.scans[idx].radials.empty()) {
            out_elev = product.scans[idx].elevation;
            return true;
        }
    }
    return false;
}

} // namespace

float sidebar_width_for_window(int width) {
    if (width <= 0) return kSidebarWidth;
    return std::min(kSidebarWidth, std::max(0.0f, static_cast<float>(width) - kMinRadarWidth));
}

void draw_sidebar(GLFWwindow *window, AppState &app) {
    ViewState &view = app.view;
    int window_width = 0;
    int window_height = 0;
    glfwGetWindowSize(window, &window_width, &window_height);
    const float sidebar_width = sidebar_width_for_window(window_width);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(sidebar_width, static_cast<float>(window_height)));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("Controls", nullptr, flags);

    if (title_font()) ImGui::PushFont(title_font());
    ImGui::TextUnformatted("OpenReflectivity");
    if (title_font()) ImGui::PopFont();

    if (!app.site.site_id.empty()) {
        if (app.site.vcp > 0) {
            ImGui::Text("%s  VCP %d", app.site.site_id.c_str(), app.site.vcp);
        } else {
            ImGui::TextUnformatted(app.site.site_id.c_str());
        }
    }
    if (const rsl::ScanTime *t = find_volume_time(app)) {
        ImGui::TextDisabled("%04d-%02d-%02d %02d:%02d:%02.0f UTC",
                            t->year, t->month, t->day, t->hour, t->minute,
                            static_cast<double>(t->sec));
    }
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("PRODUCT");
    for (const ProductDescriptor &desc : product_table()) {
        const bool available = !app.products[product_index(desc.type)].scans.empty();
        const bool selected = view.requested_product == desc.type;
        if (!available) ImGui::BeginDisabled();
        if (ImGui::Selectable(desc.label, selected)) {
            view.requested_product = desc.type;
        }
        if (!available) {
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("No scans available");
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("ELEVATION");

    const size_t pi = product_index(view.current_product);
    const std::vector<rsl::Scan> &scans = app.products[pi].scans;
    const int scan_count = static_cast<int>(scans.size());
    const int requested_idx = view.clamp_scan_index(view.requested_scan_idx);
    const ImGuiStyle &style = ImGui::GetStyle();
    const float button_width =
        std::max(0.0f, (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x * 2.0f) / 3.0f);

    const bool can_step_prev = scan_count > 1 && requested_idx > 0;
    const bool can_step_next = scan_count > 1 && requested_idx < scan_count - 1;
    if (!can_step_prev) ImGui::BeginDisabled();
    if (ImGui::Button("Prev", ImVec2(button_width, 0.0f))) {
        view.request_scan_delta(-1);
        view.last_playback_advance_time = glfwGetTime();
    }
    if (!can_step_prev) ImGui::EndDisabled();

    ImGui::SameLine();
    const bool can_toggle_playback = scan_count > 1 || view.playback_active;
    if (!can_toggle_playback) ImGui::BeginDisabled();
    if (ImGui::Button(view.playback_active ? "Pause" : "Play", ImVec2(button_width, 0.0f))) {
        view.playback_active = !view.playback_active;
        view.last_playback_advance_time = glfwGetTime();
    }
    if (!can_toggle_playback) ImGui::EndDisabled();

    ImGui::SameLine();
    if (!can_step_next) ImGui::BeginDisabled();
    if (ImGui::Button("Next", ImVec2(button_width, 0.0f))) {
        view.request_scan_delta(1);
        view.last_playback_advance_time = glfwGetTime();
    }
    if (!can_step_next) ImGui::EndDisabled();

    if (scan_count > 0) {
        const float row_h = ImGui::GetTextLineHeightWithSpacing();
        const float list_h = row_h * std::min(8.5f, static_cast<float>(scan_count) + 0.5f);
        ImGui::BeginChild("tilt_list", ImVec2(0.0f, list_h), ImGuiChildFlags_Borders);
        static int last_scrolled_idx = -1;
        for (int i = 0; i < scan_count; ++i) {
            const bool selected = i == requested_idx;
            const bool has_data = !scans[static_cast<size_t>(i)].radials.empty();
            float elev = 0.0f;
            char label[48];
            if (elevation_for_index(app, static_cast<size_t>(i), elev)) {
                std::snprintf(label, sizeof(label), "%4.1f deg%s##tilt%d",
                              elev, has_data ? "" : "  (no data)", i);
            } else {
                std::snprintf(label, sizeof(label), "  -- ##tilt%d", i);
            }
            if (ImGui::Selectable(label, selected)) {
                view.requested_scan_idx = i;
                view.last_playback_advance_time = glfwGetTime();
            }
            if (selected && view.scan_idx != last_scrolled_idx) {
                ImGui::SetScrollHereY(0.5f);
                last_scrolled_idx = view.scan_idx;
            }
        }
        ImGui::EndChild();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("SPEED");
    view.playback_sweeps_per_second =
        std::clamp(view.playback_sweeps_per_second,
                   kMinPlaybackSweepsPerSecond, kMaxPlaybackSweepsPerSecond);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::SliderFloat("##playback_speed", &view.playback_sweeps_per_second,
                           kMinPlaybackSweepsPerSecond, kMaxPlaybackSweepsPerSecond,
                           "%.2fx", ImGuiSliderFlags_AlwaysClamp)) {
        view.last_playback_advance_time = glfwGetTime();
    }
    if (ImGui::IsItemHovered()) {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            view.playback_sweeps_per_second =
                std::clamp(view.playback_sweeps_per_second + wheel * kPlaybackSpeedWheelStep,
                           kMinPlaybackSweepsPerSecond, kMaxPlaybackSweepsPerSecond);
            view.last_playback_advance_time = glfwGetTime();
        }
    }

    const rsl::Scan *active_scan = nullptr;
    if (!scans.empty()) {
        const int active_idx = view.clamp_scan_index(view.scan_idx);
        active_scan = &scans[static_cast<size_t>(active_idx)];
    }

    if (view.current_product == rsl::ProductType::VELOCITY) {
        ImGui::Spacing();
        ImGui::TextDisabled("STORM MOTION");
        ImGui::Checkbox("Storm relative", &app.srv_enabled);
        if (app.srv_enabled) {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##storm_from", &app.storm_from_deg, 1.0f, 0.0f, 360.0f,
                             "From %.0f deg", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##storm_speed", &app.storm_speed_kt, 0.5f, 0.0f, 100.0f,
                             "%.0f kt", ImGuiSliderFlags_AlwaysClamp);
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("Screenshot (P)", ImVec2(-1.0f, 0.0f))) {
        app.screenshot_requested = true;
    }

    ImGui::Separator();
    ImGui::Text("Active: %s", product_label(view.current_product));
    ImGui::Text("Sweep: %d / %d", view.scan_idx + 1, app.product_scan_count(view.current_product));
    if (active_scan) {
        ImGui::Text("Elevation: %.2f deg", active_scan->elevation);
        if (view.current_product == rsl::ProductType::VELOCITY && active_scan->nyquist_vel > 0.0f) {
            ImGui::Text("Nyquist: %.1f m/s", active_scan->nyquist_vel);
        }
    }

    ImGui::End();
}
