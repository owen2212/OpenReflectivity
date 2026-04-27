#include <cstdio>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "app/radar_render_data.hpp"
#include "app/renderers.hpp"
#include "app/view_state.hpp"
#include "rsl/rsl_wrapper.hpp"

namespace {

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

void cursor_pos_callback(GLFWwindow *win, double xpos, double ypos) {
    ViewState *view = static_cast<ViewState*>(glfwGetWindowUserPointer(win));
    if (!view) return;
    if (view->dragging) {
        int width = 0;
        int height = 0;
        glfwGetWindowSize(win, &width, &height);
        view->pan_by_pixels(xpos - view->last_cursor_x, ypos - view->last_cursor_y,
                            width, height);
    }
    view->last_cursor_x = xpos;
    view->last_cursor_y = ypos;
}

void mouse_button_callback(GLFWwindow *win, int button, int action, int /*mods*/) {
    ViewState *view = static_cast<ViewState*>(glfwGetWindowUserPointer(win));
    if (!view || button != GLFW_MOUSE_BUTTON_LEFT) return;
    if (action == GLFW_PRESS) {
        view->dragging = true;
        glfwGetCursorPos(win, &view->last_cursor_x, &view->last_cursor_y);
    } else if (action == GLFW_RELEASE) {
        view->dragging = false;
    }
}

void scroll_callback(GLFWwindow *win, double /*xoff*/, double yoff) {
    ViewState *view = static_cast<ViewState*>(glfwGetWindowUserPointer(win));
    if (!view) return;

    int width = 0;
    int height = 0;
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    glfwGetWindowSize(win, &width, &height);
    glfwGetCursorPos(win, &cursor_x, &cursor_y);
    view->zoom_at(yoff, cursor_x, cursor_y, width, height);
}

void key_callback(GLFWwindow *win, int key, int /*sc*/, int action, int /*mods*/) {
    ViewState *view = static_cast<ViewState*>(glfwGetWindowUserPointer(win));
    if (!view) return;
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    if (action == GLFW_PRESS && key == GLFW_KEY_R) {
        view->reset_view();
    } else if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
    } else if (key == GLFW_KEY_LEFT_BRACKET) {
        view->request_scan_delta(-1);
    } else if (key == GLFW_KEY_RIGHT_BRACKET) {
        view->request_scan_delta(1);
    }
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

    rsl::Product product;
    try {
        rsl::RadarData radar_data(config.level2_path, config.site_id);
        product = radar_data.get_product(config.product_type);
    } catch (const std::exception &ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }

    if (product.scans.empty()) {
        std::fprintf(stderr, "No scans in product '%s'\n", config.product_name.c_str());
        return 1;
    }

    GLFWwindow *window = nullptr;
    if (!initialize_glfw_window(window)) {
        return 1;
    }

    ViewState view;
    view.num_scans = static_cast<int>(product.scans.size());
    glfwSetWindowUserPointer(window, &view);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    std::printf("Loaded %d sweeps from %s (%s). Use [ and ] to switch elevation, R to reset view, Esc to quit.\n",
                view.num_scans, config.site_id.c_str(), config.product_name.c_str());
    std::printf("Active sweep: 0 (elevation %.2f deg)\n", product.scans[0].elevation);

    int exit_code = 0;
    {
        std::vector<std::optional<ScanGpuData>> scan_cache(product.scans.size());
        scan_cache[0] = build_scan_gpu_data(product.scans[0]);

        Texture dbz_lut = create_dbz_lut_texture();
        ReflectivityRenderer reflectivity_renderer;
        OverlayRenderer overlay_renderer;
        LegendRenderer legend_renderer;

        if (!reflectivity_renderer.initialize()) {
            std::fprintf(stderr, "Failed to initialize reflectivity renderer\n");
            exit_code = 1;
        } else {
            reflectivity_renderer.upload_scan(*scan_cache[0]);
        }

        if (exit_code == 0 && !overlay_renderer.initialize(reflectivity_renderer.max_range())) {
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
            if (view.requested_scan_idx != view.scan_idx) {
                const int new_idx = view.clamp_scan_index(view.requested_scan_idx);
                view.scan_idx = new_idx;
                view.requested_scan_idx = new_idx;

                if (!scan_cache[static_cast<size_t>(new_idx)]) {
                    scan_cache[static_cast<size_t>(new_idx)] =
                        build_scan_gpu_data(product.scans[static_cast<size_t>(new_idx)]);
                }
                reflectivity_renderer.upload_scan(*scan_cache[static_cast<size_t>(new_idx)]);
                overlay_renderer.update_range(reflectivity_renderer.max_range());

                std::printf("Active sweep: %d (elevation %.2f deg)\n",
                            new_idx, product.scans[static_cast<size_t>(new_idx)].elevation);
                std::fflush(stdout);
            }

            int fbw = 0;
            int fbh = 0;
            glfwGetFramebufferSize(window, &fbw, &fbh);
            glViewport(0, 0, fbw, fbh);
            glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            const ViewProjection projection =
                make_view_projection(reflectivity_renderer.max_range(), fbw, fbh,
                                     view.zoom, view.offset_x, view.offset_y);
            reflectivity_renderer.draw(projection, dbz_lut);
            overlay_renderer.draw(projection);
            legend_renderer.draw(fbw, fbh, dbz_lut);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return exit_code;
}
