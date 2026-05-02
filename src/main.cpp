#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "app/app_state.hpp"
#include "app/imgui_setup.hpp"
#include "app/input.hpp"
#include "app/products.hpp"
#include "app/radar_render_data.hpp"
#include "app/renderers.hpp"
#include "app/screenshot.hpp"
#include "app/sidebar.hpp"
#include "app/view_state.hpp"
#include "rsl/rsl_wrapper.hpp"

namespace {

struct AppConfig {
    std::string level2_path = "examples/KTLX20130520_000122_V06";
    std::string site_id = "KTLX";
    std::string product_name = "reflectivity";
    rsl::ProductType product_type = rsl::ProductType::REFLECTIVITY;
};

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

    AppState app;
    try {
        rsl::RadarData radar_data(config.level2_path, config.site_id);
        app.site = radar_data.site_info();
        // The CLI-specified product must load — failure here is fatal.
        app.products[product_index(config.product_type)] = radar_data.get_product(config.product_type);
        // Best-effort load the other moments so runtime switching works.
        for (const ProductDescriptor &desc : product_table()) {
            if (desc.type == config.product_type) continue;
            try {
                app.products[product_index(desc.type)] = radar_data.get_product(desc.type);
            } catch (const std::exception &ex) {
                std::fprintf(stderr, "Warning: %s product unavailable: %s\n",
                             desc.cli_name, ex.what());
            }
        }
    } catch (const std::exception &ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }

    if (app.products[product_index(config.product_type)].scans.empty()) {
        std::fprintf(stderr, "No scans in product '%s'\n", config.product_name.c_str());
        return 1;
    }

    GLFWwindow *window = nullptr;
    if (!initialize_glfw_window(window)) {
        return 1;
    }

    ViewState &view = app.view;
    view.num_scans = app.product_scan_count(config.product_type);
    view.current_product = config.product_type;
    view.requested_product = config.product_type;
    install_input_callbacks(window, &app);

    if (!initialize_imgui(window)) {
        std::fprintf(stderr, "Failed to initialize Dear ImGui\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::printf("Site %s: lat %.4f lon %.4f height %.0f m, VCP %d\n",
                app.site.site_id.c_str(), app.site.lat, app.site.lon,
                app.site.height_m, app.site.vcp);
    std::printf("Loaded %d sweeps from %s. Initial product: %s.\n"
                "[ and ] switch elevation. Sidebar Play loops sweeps with adjustable speed.\n"
                "1/2/3 switch product (ref / vel / sw).\n"
                "R resets view, Esc quits.\n",
                view.num_scans, config.site_id.c_str(), product_name(view.current_product));
    std::printf("Active: %s sweep 0 (elevation %.2f deg)\n",
                product_name(view.current_product),
                app.products[product_index(view.current_product)].scans[0].elevation);
    std::fflush(stdout);

    int exit_code = 0;
    {
        for (size_t i = 0; i < kProductCount; ++i) {
            app.scan_caches[i].resize(app.products[i].scans.size());
        }

        const size_t initial_pi = product_index(view.current_product);
        app.scan_caches[initial_pi][0] = build_scan_gpu_data(app.products[initial_pi].scans[0]);

        for (const ProductDescriptor &desc : product_table()) {
            const size_t i = product_index(desc.type);
            app.luts[i] = make_product_lut_texture(desc.type);
            app.configs[i] = make_product_render_config(desc.type);
        }

        MomentRenderer moment_renderer;
        OverlayRenderer overlay_renderer;
        LegendRenderer legend_renderer;

        if (!moment_renderer.initialize()) {
            std::fprintf(stderr, "Failed to initialize moment renderer\n");
            exit_code = 1;
        } else {
            moment_renderer.upload_scan(*app.scan_caches[initial_pi][0]);
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

        const char *autoshot_path = std::getenv("OPENREFL_AUTOSHOT");
        constexpr long kAutoshotFrame = 30;
        long frame_counter = 0;

        while (exit_code == 0 && !glfwWindowShouldClose(window)) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            draw_sidebar(window, app);

            const double current_time = glfwGetTime();
            const bool product_changed = view.requested_product != view.current_product;
            const float playback_sweeps_per_second =
                std::clamp(view.playback_sweeps_per_second,
                           kMinPlaybackSweepsPerSecond, kMaxPlaybackSweepsPerSecond);
            const double playback_interval_seconds =
                1.0 / static_cast<double>(playback_sweeps_per_second);
            if (!product_changed && view.playback_active && view.num_scans > 1 &&
                current_time - view.last_playback_advance_time >= playback_interval_seconds) {
                view.request_scan_delta_wrapped(1);
                view.last_playback_advance_time = current_time;
            }

            const bool scan_changed = view.requested_scan_idx != view.scan_idx;

            if (product_changed || scan_changed) {
                if (product_changed) {
                    const rsl::ProductType new_pt = view.requested_product;
                    const size_t new_pi = product_index(new_pt);
                    if (app.products[new_pi].scans.empty()) {
                        std::fprintf(stderr,
                                     "Product %s has no scans; staying on %s.\n",
                                     product_name(new_pt), product_name(view.current_product));
                        view.requested_product = view.current_product;
                    } else {
                        view.current_product = new_pt;
                        view.num_scans = static_cast<int>(app.products[new_pi].scans.size());
                    }
                    view.last_playback_advance_time = current_time;
                }

                const int new_idx = view.clamp_scan_index(view.requested_scan_idx);
                view.scan_idx = new_idx;
                view.requested_scan_idx = new_idx;

                const size_t pi = product_index(view.current_product);
                auto &cache = app.scan_caches[pi];
                if (!cache[static_cast<size_t>(new_idx)]) {
                    cache[static_cast<size_t>(new_idx)] =
                        build_scan_gpu_data(app.products[pi].scans[static_cast<size_t>(new_idx)]);
                }
                const ScanGpuData &gpu = *cache[static_cast<size_t>(new_idx)];
                moment_renderer.upload_scan(gpu);
                overlay_renderer.update_range(moment_renderer.max_range());

                const rsl::Scan &scan = app.products[pi].scans[static_cast<size_t>(new_idx)];
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
            ProductRenderConfig effective = app.configs[pi];
            if (view.current_product == rsl::ProductType::VELOCITY) {
                const float nyq = app.products[pi].scans[static_cast<size_t>(view.scan_idx)].nyquist_vel;
                if (nyq > 0.0f) {
                    effective.min_value = -nyq;
                    effective.max_value = nyq;
                }
            }

            const ViewProjection projection =
                make_view_projection(moment_renderer.max_range(), radar_width_fb, fbh,
                                     view.zoom, view.offset_x, view.offset_y);
            moment_renderer.draw(projection, app.luts[pi], effective);
            overlay_renderer.draw(projection);
            legend_renderer.draw(radar_width_fb, fbh, app.luts[pi], effective);

            glViewport(0, 0, fbw, fbh);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            if (app.screenshot_requested) {
                app.screenshot_requested = false;
                save_screenshot_png(fbw, fbh);
            }
            // Developer hook: OPENREFL_AUTOSHOT=<path> captures one frame
            // shortly after startup, for non-interactive visual checks.
            if (autoshot_path && frame_counter == kAutoshotFrame) {
                save_screenshot_png_to(fbw, fbh, autoshot_path);
                std::printf("Saved autoshot: %s\n", autoshot_path);
                std::fflush(stdout);
            }
            ++frame_counter;

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    shutdown_imgui();
    glfwDestroyWindow(window);
    glfwTerminate();
    return exit_code;
}
