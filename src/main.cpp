#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "app/radar_render_data.hpp"
#include "app/renderers.hpp"
#include "app/view_state.hpp"
#include "rsl/rsl_wrapper.hpp"

namespace {

constexpr size_t kProductCount = 3;
constexpr float kSidebarWidth = 280.0f;
constexpr float kMinRadarWidth = 320.0f;

ImFont *g_title_font = nullptr;

constexpr std::array<rsl::ProductType, kProductCount> kAllProducts = {
    rsl::ProductType::REFLECTIVITY,
    rsl::ProductType::VELOCITY,
    rsl::ProductType::SPECTRAL_WIDTH,
};

size_t product_index(rsl::ProductType pt) {
    return static_cast<size_t>(pt);
}

const char* product_name(rsl::ProductType pt) {
    switch (pt) {
        case rsl::ProductType::REFLECTIVITY: return "reflectivity";
        case rsl::ProductType::VELOCITY: return "velocity";
        case rsl::ProductType::SPECTRAL_WIDTH: return "spectrum_width";
    }
    return "unknown";
}

const char* product_label(rsl::ProductType pt) {
    switch (pt) {
        case rsl::ProductType::REFLECTIVITY: return "Reflectivity";
        case rsl::ProductType::VELOCITY: return "Velocity";
        case rsl::ProductType::SPECTRAL_WIDTH: return "Spectrum Width";
    }
    return "Unknown";
}

struct AppConfig {
    std::string level2_path = "examples/KTLX20130520_000122_V06";
    std::string site_id = "KTLX";
    std::string product_name = "reflectivity";
    rsl::ProductType product_type = rsl::ProductType::REFLECTIVITY;
};

bool parse_product(const std::string &name, rsl::ProductType &out) {
    if (name == "reflectivity" || name == "ref") {
        out = rsl::ProductType::REFLECTIVITY;
        return true;
    }
    if (name == "velocity" || name == "vel") {
        out = rsl::ProductType::VELOCITY;
        return true;
    }
    if (name == "spectrum_width" || name == "spectral_width" || name == "sw") {
        out = rsl::ProductType::SPECTRAL_WIDTH;
        return true;
    }
    return false;
}

bool parse_args(int argc, char **argv, AppConfig &config) {
    if (argc > 1) config.level2_path = argv[1];
    if (argc > 2) config.site_id = argv[2];
    if (argc > 3) config.product_name = argv[3];
    if (argc > 4) {
        std::fprintf(stderr, "Usage: %s [level2-file] [site-id] [product]\n", argv[0]);
        return false;
    }
    if (!parse_product(config.product_name, config.product_type)) {
        std::fprintf(stderr,
                     "Unsupported product '%s'. Use reflectivity, velocity, or spectrum_width.\n",
                     config.product_name.c_str());
        return false;
    }
    return true;
}

float sidebar_width_for_window(int width) {
    if (width <= 0) return kSidebarWidth;
    return std::min(kSidebarWidth, std::max(0.0f, static_cast<float>(width) - kMinRadarWidth));
}

int product_scan_count(const std::array<rsl::Product, kProductCount> &products,
                       rsl::ProductType product_type) {
    return static_cast<int>(products[product_index(product_type)].scans.size());
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

void draw_sidebar(GLFWwindow *window, ViewState &view,
                  const std::array<rsl::Product, kProductCount> &products) {
    int window_width = 0;
    int window_height = 0;
    glfwGetWindowSize(window, &window_width, &window_height);
    const float sidebar_width = sidebar_width_for_window(window_width);

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(sidebar_width, static_cast<float>(window_height)));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("Controls", nullptr, flags);

    if (g_title_font) ImGui::PushFont(g_title_font);
    ImGui::TextUnformatted("OpenReflectivity");
    if (g_title_font) ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("PRODUCT");
    for (rsl::ProductType pt : kAllProducts) {
        const bool available = !products[product_index(pt)].scans.empty();
        const bool selected = view.requested_product == pt;
        if (!available) ImGui::BeginDisabled();
        if (ImGui::Selectable(product_label(pt), selected)) {
            view.requested_product = pt;
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
    const std::vector<rsl::Scan> &scans = products[pi].scans;
    const float list_height = std::max(140.0f, ImGui::GetContentRegionAvail().y - 96.0f);
    if (ImGui::BeginChild("sweeps", ImVec2(0.0f, list_height), true)) {
        for (size_t i = 0; i < scans.size(); ++i) {
            const rsl::Scan &scan = scans[i];
            char label[96];
            if (scan.radials.empty()) {
                std::snprintf(label, sizeof(label), "%02zu  %.2f deg  no data", i, scan.elevation);
            } else {
                std::snprintf(label, sizeof(label), "%02zu  %.2f deg", i, scan.elevation);
            }
            if (ImGui::Selectable(label, view.requested_scan_idx == static_cast<int>(i))) {
                view.requested_scan_idx = static_cast<int>(i);
            }
        }
    }
    ImGui::EndChild();

    const rsl::Scan *active_scan = nullptr;
    if (!scans.empty()) {
        const int active_idx = view.clamp_scan_index(view.scan_idx);
        active_scan = &scans[static_cast<size_t>(active_idx)];
    }

    ImGui::Separator();
    ImGui::Text("Active: %s", product_label(view.current_product));
    ImGui::Text("Sweep: %d / %d", view.scan_idx + 1, product_scan_count(products, view.current_product));
    if (active_scan) {
        ImGui::Text("Elevation: %.2f deg", active_scan->elevation);
        if (view.current_product == rsl::ProductType::VELOCITY && active_scan->nyquist_vel > 0.0f) {
            ImGui::Text("Nyquist: %.1f m/s", active_scan->nyquist_vel);
        }
    }

    ImGui::End();
}

void cursor_pos_callback(GLFWwindow *win, double xpos, double ypos) {
    ImGui_ImplGlfw_CursorPosCallback(win, xpos, ypos);
    ViewState *view = static_cast<ViewState*>(glfwGetWindowUserPointer(win));
    if (!view) return;
    if (ImGui::GetIO().WantCaptureMouse) {
        view->last_cursor_x = xpos;
        view->last_cursor_y = ypos;
        return;
    }
    if (view->dragging) {
        int width = 0;
        int height = 0;
        glfwGetWindowSize(win, &width, &height);
        const int radar_width = static_cast<int>(std::max(1.0f, static_cast<float>(width) - sidebar_width_for_window(width)));
        view->pan_by_pixels(xpos - view->last_cursor_x, ypos - view->last_cursor_y,
                            radar_width, height);
    }
    view->last_cursor_x = xpos;
    view->last_cursor_y = ypos;
}

void mouse_button_callback(GLFWwindow *win, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(win, button, action, mods);
    ViewState *view = static_cast<ViewState*>(glfwGetWindowUserPointer(win));
    if (!view || button != GLFW_MOUSE_BUTTON_LEFT) return;
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    int width = 0;
    int height = 0;
    glfwGetCursorPos(win, &cursor_x, &cursor_y);
    glfwGetWindowSize(win, &width, &height);
    if (ImGui::GetIO().WantCaptureMouse || cursor_x < sidebar_width_for_window(width)) {
        view->dragging = false;
        return;
    }
    if (action == GLFW_PRESS) {
        view->dragging = true;
        view->last_cursor_x = cursor_x;
        view->last_cursor_y = cursor_y;
    } else if (action == GLFW_RELEASE) {
        view->dragging = false;
    }
}

void scroll_callback(GLFWwindow *win, double xoff, double yoff) {
    ImGui_ImplGlfw_ScrollCallback(win, xoff, yoff);
    ViewState *view = static_cast<ViewState*>(glfwGetWindowUserPointer(win));
    if (!view) return;
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
    view->zoom_at(yoff, cursor_x - sidebar_width, cursor_y, radar_width, height);
}

void key_callback(GLFWwindow *win, int key, int sc, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(win, key, sc, action, mods);
    ViewState *view = static_cast<ViewState*>(glfwGetWindowUserPointer(win));
    if (!view) return;
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    if (action == GLFW_PRESS && key == GLFW_KEY_R) {
        view->reset_view();
    } else if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
    } else if (key == GLFW_KEY_LEFT_BRACKET) {
        view->request_scan_delta(-1);
    } else if (key == GLFW_KEY_RIGHT_BRACKET) {
        view->request_scan_delta(1);
    } else if (action == GLFW_PRESS && key == GLFW_KEY_1) {
        view->requested_product = rsl::ProductType::REFLECTIVITY;
    } else if (action == GLFW_PRESS && key == GLFW_KEY_2) {
        view->requested_product = rsl::ProductType::VELOCITY;
    } else if (action == GLFW_PRESS && key == GLFW_KEY_3) {
        view->requested_product = rsl::ProductType::SPECTRAL_WIDTH;
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

bool initialize_glfw_window(GLFWwindow *&window) {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window = glfwCreateWindow(1024, 768, "OpenReflectivity", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "Failed to initialize GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        window = nullptr;
        return false;
    }

    std::printf("OpenGL Vendor  : %s\n", glGetString(GL_VENDOR));
    std::printf("OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
    std::printf("OpenGL Version : %s\n", glGetString(GL_VERSION));
    return true;
}

} // namespace

int main(int argc, char **argv) {
    AppConfig config;
    if (!parse_args(argc, argv, config)) {
        return 1;
    }

    std::array<rsl::Product, kProductCount> products;
    try {
        rsl::RadarData radar_data(config.level2_path, config.site_id);
        // The CLI-specified product must load — failure here is fatal.
        products[product_index(config.product_type)] = radar_data.get_product(config.product_type);
        // Best-effort load the other moments so runtime switching works.
        for (rsl::ProductType pt : kAllProducts) {
            if (pt == config.product_type) continue;
            try {
                products[product_index(pt)] = radar_data.get_product(pt);
            } catch (const std::exception &ex) {
                std::fprintf(stderr, "Warning: %s product unavailable: %s\n",
                             product_name(pt), ex.what());
            }
        }
    } catch (const std::exception &ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }

    if (products[product_index(config.product_type)].scans.empty()) {
        std::fprintf(stderr, "No scans in product '%s'\n", config.product_name.c_str());
        return 1;
    }

    GLFWwindow *window = nullptr;
    if (!initialize_glfw_window(window)) {
        return 1;
    }

    ViewState view;
    view.num_scans = static_cast<int>(products[product_index(config.product_type)].scans.size());
    view.current_product = config.product_type;
    view.requested_product = config.product_type;
    glfwSetWindowUserPointer(window, &view);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetCursorEnterCallback(window, cursor_enter_callback);
    glfwSetWindowFocusCallback(window, window_focus_callback);

    if (!initialize_imgui(window)) {
        std::fprintf(stderr, "Failed to initialize Dear ImGui\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::printf("Loaded %d sweeps from %s. Initial product: %s.\n"
                "[ and ] switch elevation. 1/2/3 switch product (ref / vel / sw).\n"
                "R resets view, Esc quits.\n",
                view.num_scans, config.site_id.c_str(), product_name(view.current_product));
    std::printf("Active: %s sweep 0 (elevation %.2f deg)\n",
                product_name(view.current_product),
                products[product_index(view.current_product)].scans[0].elevation);

    int exit_code = 0;
    {
        std::array<std::vector<std::optional<ScanGpuData>>, kProductCount> scan_caches;
        for (size_t i = 0; i < kProductCount; ++i) {
            scan_caches[i].resize(products[i].scans.size());
        }

        const size_t initial_pi = product_index(view.current_product);
        scan_caches[initial_pi][0] = build_scan_gpu_data(products[initial_pi].scans[0]);

        std::array<Texture, kProductCount> luts;
        std::array<ProductRenderConfig, kProductCount> configs;
        for (rsl::ProductType pt : kAllProducts) {
            const size_t i = product_index(pt);
            luts[i] = make_product_lut_texture(pt);
            configs[i] = make_product_render_config(pt);
        }

        MomentRenderer moment_renderer;
        OverlayRenderer overlay_renderer;
        LegendRenderer legend_renderer;

        if (!moment_renderer.initialize()) {
            std::fprintf(stderr, "Failed to initialize moment renderer\n");
            exit_code = 1;
        } else {
            moment_renderer.upload_scan(*scan_caches[initial_pi][0]);
        }

        if (exit_code == 0 && !overlay_renderer.initialize(moment_renderer.max_range())) {
            std::fprintf(stderr, "Failed to initialize overlay renderer\n");
            exit_code = 1;
        }

        if (exit_code == 0 && !legend_renderer.initialize()) {
            std::fprintf(stderr, "Failed to initialize legend renderer\n");
            exit_code = 1;
        }

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(1.0f);

        while (exit_code == 0 && !glfwWindowShouldClose(window)) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            draw_sidebar(window, view, products);

            const bool product_changed = view.requested_product != view.current_product;
            const bool scan_changed = view.requested_scan_idx != view.scan_idx;

            if (product_changed || scan_changed) {
                if (product_changed) {
                    const rsl::ProductType new_pt = view.requested_product;
                    const size_t new_pi = product_index(new_pt);
                    if (products[new_pi].scans.empty()) {
                        std::fprintf(stderr,
                                     "Product %s has no scans; staying on %s.\n",
                                     product_name(new_pt), product_name(view.current_product));
                        view.requested_product = view.current_product;
                    } else {
                        view.current_product = new_pt;
                        view.num_scans = static_cast<int>(products[new_pi].scans.size());
                    }
                }

                const int new_idx = view.clamp_scan_index(view.requested_scan_idx);
                view.scan_idx = new_idx;
                view.requested_scan_idx = new_idx;

                const size_t pi = product_index(view.current_product);
                auto &cache = scan_caches[pi];
                if (!cache[static_cast<size_t>(new_idx)]) {
                    cache[static_cast<size_t>(new_idx)] =
                        build_scan_gpu_data(products[pi].scans[static_cast<size_t>(new_idx)]);
                }
                const ScanGpuData &gpu = *cache[static_cast<size_t>(new_idx)];
                moment_renderer.upload_scan(gpu);
                overlay_renderer.update_range(moment_renderer.max_range());

                const rsl::Scan &scan = products[pi].scans[static_cast<size_t>(new_idx)];
                if (gpu.gates.empty()) {
                    std::printf("[no data] %s sweep %d\n",
                                product_name(view.current_product), new_idx);
                } else if (view.current_product == rsl::ProductType::VELOCITY && scan.nyquist_vel > 0.0f) {
                    std::printf("Active: %s sweep %d (elevation %.2f deg, nyquist %.1f m/s)\n",
                                product_name(view.current_product), new_idx,
                                scan.elevation, scan.nyquist_vel);
                } else {
                    std::printf("Active: %s sweep %d (elevation %.2f deg)\n",
                                product_name(view.current_product), new_idx, scan.elevation);
                }
                std::fflush(stdout);
            }

            int fbw = 0;
            int fbh = 0;
            int ww = 0;
            int wh = 0;
            glfwGetFramebufferSize(window, &fbw, &fbh);
            glfwGetWindowSize(window, &ww, &wh);
            const float framebuffer_scale_x = (ww > 0) ? static_cast<float>(fbw) / static_cast<float>(ww) : 1.0f;
            const int sidebar_width_fb =
                static_cast<int>(std::round(sidebar_width_for_window(ww) * framebuffer_scale_x));
            const int radar_width_fb = std::max(1, fbw - sidebar_width_fb);

            glViewport(0, 0, fbw, fbh);
            glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glViewport(sidebar_width_fb, 0, radar_width_fb, fbh);

            const size_t pi = product_index(view.current_product);
            ProductRenderConfig effective = configs[pi];
            if (view.current_product == rsl::ProductType::VELOCITY) {
                const float nyq = products[pi].scans[static_cast<size_t>(view.scan_idx)].nyquist_vel;
                if (nyq > 0.0f) {
                    effective.min_value = -nyq;
                    effective.max_value = nyq;
                }
            }

            const ViewProjection projection =
                make_view_projection(moment_renderer.max_range(), radar_width_fb, fbh,
                                     view.zoom, view.offset_x, view.offset_y);
            moment_renderer.draw(projection, luts[pi], effective);
            overlay_renderer.draw(projection);
            legend_renderer.draw(radar_width_fb, fbh, luts[pi], effective);

            glViewport(0, 0, fbw, fbh);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    shutdown_imgui();
    glfwDestroyWindow(window);
    glfwTerminate();
    return exit_code;
}
