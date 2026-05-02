#include "sidebar.hpp"

#include <algorithm>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "app/app_state.hpp"
#include "app/imgui_setup.hpp"

namespace {

constexpr float kSidebarWidth = 280.0f;
constexpr float kMinRadarWidth = 320.0f;
constexpr float kPlaybackSpeedWheelStep = 0.25f;

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
