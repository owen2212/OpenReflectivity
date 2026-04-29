#include <array>
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

constexpr size_t kProductCount = 3;

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
    } else if (action == GLFW_PRESS && key == GLFW_KEY_1) {
        view->requested_product = rsl::ProductType::REFLECTIVITY;
    } else if (action == GLFW_PRESS && key == GLFW_KEY_2) {
        view->requested_product = rsl::ProductType::VELOCITY;
    } else if (action == GLFW_PRESS && key == GLFW_KEY_3) {
        view->requested_product = rsl::ProductType::SPECTRAL_WIDTH;
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
            glfwGetFramebufferSize(window, &fbw, &fbh);
            glViewport(0, 0, fbw, fbh);
            glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

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
                make_view_projection(moment_renderer.max_range(), fbw, fbh,
                                     view.zoom, view.offset_x, view.offset_y);
            moment_renderer.draw(projection, luts[pi], effective);
            overlay_renderer.draw(projection);
            legend_renderer.draw(fbw, fbh, luts[pi], effective);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return exit_code;
}
