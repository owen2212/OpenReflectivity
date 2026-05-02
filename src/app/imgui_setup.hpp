#ifndef IMGUI_SETUP_HPP
#define IMGUI_SETUP_HPP

struct GLFWwindow;
struct ImFont;

void apply_grlevelx_style();
void configure_imgui_font(GLFWwindow *window);
bool initialize_imgui(GLFWwindow *window);
void shutdown_imgui();

// Larger Roboto face for the sidebar title; nullptr if the TTF was missing.
ImFont* title_font();

#endif
