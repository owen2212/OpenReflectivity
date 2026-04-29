#ifndef VIEW_STATE_HPP
#define VIEW_STATE_HPP

#include "rsl/rsl_wrapper.hpp"

struct ViewState {
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float zoom = 1.0f;
    bool dragging = false;
    double last_cursor_x = 0.0;
    double last_cursor_y = 0.0;

    int scan_idx = 0;
    int requested_scan_idx = 0;
    int num_scans = 1;
    bool playback_active = false;
    double last_playback_advance_time = 0.0;
    float playback_sweeps_per_second = 1.0f;

    rsl::ProductType current_product = rsl::ProductType::REFLECTIVITY;
    rsl::ProductType requested_product = rsl::ProductType::REFLECTIVITY;

    void reset_view();
    void pan_by_pixels(double dx, double dy, int width, int height);
    void zoom_at(double y_offset, double cursor_x, double cursor_y, int width, int height);
    void request_scan_delta(int delta);
    void request_scan_delta_wrapped(int delta);
    int clamp_scan_index(int idx) const;
};

#endif
