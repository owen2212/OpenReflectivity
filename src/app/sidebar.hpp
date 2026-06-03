#ifndef SIDEBAR_HPP
#define SIDEBAR_HPP

struct GLFWwindow;
struct AppState;
class VolumeSet;

// Single source of truth for the sidebar width; input gating and viewport
// math must both use this.
float sidebar_width_for_window(int window_width);

void draw_sidebar(GLFWwindow *window, AppState &app, const VolumeSet &volumes);

#endif
