#include "app/view_state.hpp"

#include <cmath>

void ViewState::reset_view() {
    offset_x = 0.0f;
    offset_y = 0.0f;
    zoom = 1.0f;
}

void ViewState::pan_by_pixels(double dx, double dy, int width, int height) {
    if (width > 0) offset_x += static_cast<float>(2.0 * dx / width);
    if (height > 0) offset_y -= static_cast<float>(2.0 * dy / height);
}

void ViewState::zoom_at(double y_offset, double cursor_x, double cursor_y, int width, int height) {
    const float ncx = (width > 0) ? static_cast<float>(2.0 * cursor_x / width - 1.0) : 0.0f;
    const float ncy = (height > 0) ? static_cast<float>(1.0 - 2.0 * cursor_y / height) : 0.0f;
    float new_zoom = zoom * std::pow(1.1f, static_cast<float>(y_offset));
    if (new_zoom < 0.1f) new_zoom = 0.1f;
    if (new_zoom > 50.0f) new_zoom = 50.0f;

    const float factor = new_zoom / zoom;
    offset_x = ncx * (1.0f - factor) + offset_x * factor;
    offset_y = ncy * (1.0f - factor) + offset_y * factor;
    zoom = new_zoom;
}

void ViewState::request_scan_delta(int delta) {
    requested_scan_idx = clamp_scan_index(requested_scan_idx + delta);
}

void ViewState::request_scan_delta_wrapped(int delta) {
    if (num_scans <= 0) {
        requested_scan_idx = 0;
        return;
    }

    const int current_idx = clamp_scan_index(requested_scan_idx);
    int next_idx = (current_idx + delta) % num_scans;
    if (next_idx < 0) next_idx += num_scans;
    requested_scan_idx = next_idx;
}

int ViewState::clamp_scan_index(int idx) const {
    if (idx < 0) return 0;
    if (idx >= num_scans) return num_scans - 1;
    return idx;
}
