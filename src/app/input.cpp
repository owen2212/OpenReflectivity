#include "input.hpp"

#include <algorithm>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>

#include "app/app_state.hpp"
#include "app/sidebar.hpp"

namespace {

AppState* app_from_window(GLFWwindow *win) {
    return static_cast<AppState*>(glfwGetWindowUserPointer(win));
}

void cursor_pos_callback(GLFWwindow *win, double xpos, double ypos) {
    ImGui_ImplGlfw_CursorPosCallback(win, xpos, ypos);
    AppState *app = app_from_window(win);
    if (!app) return;
    ViewState &view = app->view;
    if (ImGui::GetIO().WantCaptureMouse) {
        view.last_cursor_x = xpos;
        view.last_cursor_y = ypos;
        return;
    }
    if (view.dragging) {
        int width = 0;
        int height = 0;
        glfwGetWindowSize(win, &width, &height);
        const int radar_width = static_cast<int>(std::max(1.0f, static_cast<float>(width) - sidebar_width_for_window(width)));
        view.pan_by_pixels(xpos - view.last_cursor_x, ypos - view.last_cursor_y,
                           radar_width, height);
    }
    view.last_cursor_x = xpos;
    view.last_cursor_y = ypos;
}

void mouse_button_callback(GLFWwindow *win, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(win, button, action, mods);
    AppState *app = app_from_window(win);
    if (!app || button != GLFW_MOUSE_BUTTON_LEFT) return;
    ViewState &view = app->view;
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    int width = 0;
    int height = 0;
    glfwGetCursorPos(win, &cursor_x, &cursor_y);
    glfwGetWindowSize(win, &width, &height);
    if (ImGui::GetIO().WantCaptureMouse || cursor_x < sidebar_width_for_window(width)) {
        view.dragging = false;
        return;
    }
    if (action == GLFW_PRESS) {
        view.dragging = true;
        view.last_cursor_x = cursor_x;
        view.last_cursor_y = cursor_y;
    } else if (action == GLFW_RELEASE) {
        view.dragging = false;
    }
}

void scroll_callback(GLFWwindow *win, double xoff, double yoff) {
    ImGui_ImplGlfw_ScrollCallback(win, xoff, yoff);
    AppState *app = app_from_window(win);
    if (!app) return;
    if (ImGui::GetIO().WantCaptureMouse) return;

    int width = 0;
    int height = 0;
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    glfwGetWindowSize(win, &width, &height);
    glfwGetCursorPos(win, &cursor_x, &cursor_y);
    const float sidebar_width = sidebar_width_for_window(width);
    if (cursor_x < sidebar_width) return;
    const int radar_width = static_cast<int>(std::max(1.0f, static_cast<float>(width) - sidebar_width));
    app->view.zoom_at(yoff, cursor_x - sidebar_width, cursor_y, radar_width, height);
}

void key_callback(GLFWwindow *win, int key, int sc, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(win, key, sc, action, mods);
    AppState *app = app_from_window(win);
    if (!app) return;
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    ViewState &view = app->view;

    if (action == GLFW_PRESS && key == GLFW_KEY_R) {
        view.reset_view();
    } else if (action == GLFW_PRESS && key == GLFW_KEY_P) {
        app->screenshot_requested = true;
    } else if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
    } else if (key == GLFW_KEY_LEFT_BRACKET) {
        view.request_scan_delta(-1);
        view.last_playback_advance_time = glfwGetTime();
    } else if (key == GLFW_KEY_RIGHT_BRACKET) {
        view.request_scan_delta(1);
        view.last_playback_advance_time = glfwGetTime();
    } else if (key == GLFW_KEY_COMMA) {
        view.request_volume_delta(-1);
        view.last_playback_advance_time = glfwGetTime();
    } else if (key == GLFW_KEY_PERIOD) {
        view.request_volume_delta(1);
        view.last_playback_advance_time = glfwGetTime();
    } else if (action == GLFW_PRESS) {
        for (const ProductDescriptor &desc : product_table()) {
            if (key == desc.hotkey) {
                view.requested_product = desc.type;
                break;
            }
        }
    }
}

void char_callback(GLFWwindow *win, unsigned int codepoint) {
    ImGui_ImplGlfw_CharCallback(win, codepoint);
}

void cursor_enter_callback(GLFWwindow *win, int entered) {
    ImGui_ImplGlfw_CursorEnterCallback(win, entered);
}

void window_focus_callback(GLFWwindow *win, int focused) {
    ImGui_ImplGlfw_WindowFocusCallback(win, focused);
}

void drop_callback(GLFWwindow *win, int count, const char **paths) {
    AppState *app = app_from_window(win);
    if (!app || count <= 0) return;
    app->pending_drop_path = paths[0];
}

} // namespace

void install_input_callbacks(GLFWwindow *window, AppState *app) {
    glfwSetWindowUserPointer(window, app);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetCursorEnterCallback(window, cursor_enter_callback);
    glfwSetWindowFocusCallback(window, window_focus_callback);
    glfwSetDropCallback(window, drop_callback);
}
