#include "imgui_setup.hpp"

#include <algorithm>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace {
ImFont *g_title_font = nullptr;
}

ImFont* title_font() {
    return g_title_font;
}

void apply_grlevelx_style() {
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.WindowPadding = ImVec2(14.0f, 14.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 8.0f);
    style.ScrollbarSize = 12.0f;
    style.ScrollbarRounding = 4.0f;

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.23f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.23f, 0.27f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.26f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.12f, 0.38f, 0.58f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.14f, 0.38f, 0.56f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.45f, 0.66f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.12f, 0.34f, 0.52f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.34f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.46f, 0.54f, 1.00f);
}

void configure_imgui_font(GLFWwindow *window) {
    float xscale = 1.0f;
    float yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    const float font_scale = std::max(1.0f, std::max(xscale, yscale));

    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    g_title_font = nullptr;

    ImFontConfig config;
    config.OversampleH = 3;
    config.OversampleV = 2;
    config.PixelSnapH = false;

    const char *font_path = "external/imgui/misc/fonts/Roboto-Medium.ttf";
    ImFont *body = io.Fonts->AddFontFromFileTTF(font_path, 15.0f * font_scale, &config);
    if (body) {
        g_title_font = io.Fonts->AddFontFromFileTTF(font_path, 20.0f * font_scale, &config);
    } else {
        config.SizePixels = 15.0f * font_scale;
        io.Fonts->AddFontDefault(&config);
    }

    io.FontGlobalScale = 1.0f / font_scale;
}

bool initialize_imgui(GLFWwindow *window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    configure_imgui_font(window);
    apply_grlevelx_style();
    return ImGui_ImplGlfw_InitForOpenGL(window, false) &&
           ImGui_ImplOpenGL3_Init("#version 330");
}

void shutdown_imgui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
