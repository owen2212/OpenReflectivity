#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "app/app_state.hpp"
#include "app/hud.hpp"
#include "app/imgui_setup.hpp"
#include "app/input.hpp"
#include "app/map_renderer.hpp"
#include "app/products.hpp"
#include "app/radar_render_data.hpp"
#include "app/renderers.hpp"
#include "app/screenshot.hpp"
#include "app/sidebar.hpp"
#include "app/view_state.hpp"
#include "app/volume_loader.hpp"
#include "app/volume_set.hpp"
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
        std::fprintf(stderr, "Unsupported product '%s'. Available:", config.product_name.c_str());
        for (const ProductDescriptor &desc : product_table()) {
            std::fprintf(stderr, " %s", desc.cli_name);
        }
        std::fprintf(stderr, "\n");
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

constexpr float kMapRadiusM = 600000.0f;

std::vector<MapRenderer::Layer> build_map_layers(const AppState &app) {
    std::vector<MapRenderer::Layer> layers(2);
    layers[0].lines = project_lines(app.raw_counties, app.projection, kMapRadiusM);
    layers[0].r = 0.42f; layers[0].g = 0.45f; layers[0].b = 0.52f; layers[0].a = 0.85f;
    layers[1].lines = project_lines(app.raw_states, app.projection, kMapRadiusM);
    layers[1].r = 0.80f; layers[1].g = 0.83f; layers[1].b = 0.90f; layers[1].a = 1.0f;
    return layers;
}

// elevation of scan slot idx, checking every product since a NULL sweep in
// one product can still have the elevation recorded in another
bool slot_elevation(const AppState &app, size_t idx, float &out_elev) {
    for (const rsl::Product &prod : app.products) {
        if (idx < prod.scans.size() && !prod.scans[idx].radials.empty()) {
            out_elev = prod.scans[idx].elevation;
            return true;
        }
    }
    return false;
}

// Swap the target volume's decoded data in and park the previously active
// volume's data (and warm caches) back in its entry. No deep copies.
bool activate_volume(AppState &app, VolumeSet &volumes, int target, GLFWwindow *window,
                     MomentRenderer &moment_renderer, OverlayRenderer &overlay_renderer,
                     MapRenderer &map_renderer, int &smooth_uploaded_pi,
                     int &smooth_uploaded_idx) {
    if (target < 0 || target >= volumes.size()) return false;
    if (target == volumes.active()) return true;
    VolumeEntry &entry = volumes.entry(target);
    if (!entry.data) return false;

    ViewState &view = app.view;

    // remember the physical elevation so the step survives VCP changes
    float prev_elev = 0.0f;
    const bool have_prev_elev =
        view.scan_idx >= 0 && slot_elevation(app, static_cast<size_t>(view.scan_idx), prev_elev);

    // give the current volume's data and caches back to its entry
    const int prev_active = volumes.active();
    if (prev_active >= 0) {
        VolumeEntry &prev = volumes.entry(prev_active);
        prev.data = std::make_unique<LoadedVolume>();
        prev.data->path = prev.path;
        prev.data->site = app.site;
        prev.data->products = std::move(app.products);
        prev.gpu_cache = std::move(app.scan_caches);
        prev.polar_cache = std::move(app.polar_caches);
    }

    const bool site_changed =
        std::fabs(entry.data->site.lat - app.site.lat) > 1e-9 ||
        std::fabs(entry.data->site.lon - app.site.lon) > 1e-9;
    app.products = std::move(entry.data->products);
    app.site = entry.data->site;
    app.scan_caches = std::move(entry.gpu_cache);
    app.polar_caches = std::move(entry.polar_cache);
    entry.data.reset();
    for (size_t i = 0; i < kProductCount; ++i) {
        if (app.scan_caches[i].size() != app.products[i].scans.size()) {
            app.scan_caches[i].assign(app.products[i].scans.size(), std::nullopt);
        }
        if (app.polar_caches[i].size() != app.products[i].scans.size()) {
            app.polar_caches[i].assign(app.products[i].scans.size(), std::nullopt);
        }
    }
    volumes.set_active(target);
    view.volume_idx = target;
    view.requested_volume_idx = target;
    app.load_error.clear();
    smooth_uploaded_pi = -1;
    smooth_uploaded_idx = -1;

    // keep the requested product if the new volume has it, otherwise fall
    // back to the first product that decoded
    rsl::ProductType pt = view.requested_product;
    if (app.products[product_index(pt)].scans.empty()) {
        for (const ProductDescriptor &desc : product_table()) {
            if (!app.products[product_index(desc.type)].scans.empty()) {
                pt = desc.type;
                break;
            }
        }
    }
    view.current_product = pt;
    view.requested_product = pt;
    const size_t pi = product_index(pt);
    view.num_scans = app.product_scan_count(pt);

    // pick the nearest tilt to the previous elevation, not the same index
    int idx = view.clamp_scan_index(view.scan_idx);
    if (have_prev_elev) {
        float best = 1e9f;
        for (size_t i = 0; i < app.products[pi].scans.size(); ++i) {
            float elev = 0.0f;
            if (!slot_elevation(app, i, elev)) continue;
            const float d = std::fabs(elev - prev_elev);
            if (d < best) {
                best = d;
                idx = static_cast<int>(i);
            }
        }
    }
    view.scan_idx = idx;
    view.requested_scan_idx = idx;

    auto &cache = app.scan_caches[pi];
    if (!cache.empty()) {
        const size_t uidx = static_cast<size_t>(idx);
        if (!cache[uidx]) {
            cache[uidx] = build_scan_gpu_data(app.products[pi].scans[uidx]);
        }
        moment_renderer.upload_scan(*cache[uidx]);
        overlay_renderer.update_range(moment_renderer.max_range());
    }

    // reprojecting ~200k map vertices takes a few ms, skip it when stepping
    // volumes within one site so time playback stays smooth
    if (site_changed) {
        app.projection = AzimuthalEquidistant(app.site.lat, app.site.lon);
        map_renderer.set_layers(build_map_layers(app));
        app.projected_places = project_places(app.raw_places, app.projection, kMapRadiusM);
    }

    char title[160];
    std::snprintf(title, sizeof(title), "OpenReflectivity - %s - %s",
                  app.site.site_id.c_str(),
                  std::filesystem::path(entry.path).filename().string().c_str());
    glfwSetWindowTitle(window, title);

    std::printf("Active volume %d/%d: %s (site %s, VCP %d, %d sweeps of %s)\n",
                target + 1, volumes.size(), entry.path.c_str(), app.site.site_id.c_str(),
                app.site.vcp, view.num_scans, product_name(pt));
    std::fflush(stdout);
    return true;
}

} // namespace

int main(int argc, char **argv) {
    AppConfig config;
    if (!parse_args(argc, argv, config)) {
        return 1;
    }

    AppState app;

    GLFWwindow *window = nullptr;
    if (!initialize_glfw_window(window)) {
        return 1;
    }

    ViewState &view = app.view;
    view.num_scans = 0;
    view.current_product = config.product_type;
    view.requested_product = config.product_type;
    install_input_callbacks(window, &app);

    if (!initialize_imgui(window)) {
        std::fprintf(stderr, "Failed to initialize Dear ImGui\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::printf("[ and ] switch elevation. Sidebar Play loops sweeps with adjustable speed.\n"
                "1-6 switch product (ref / vel / sw / zdr / cc / phidp).\n"
                "Drop a Level 2 archive on the window to load it.\n"
                "P saves a screenshot. R resets view, Esc quits.\n");
    std::fflush(stdout);

    int exit_code = 0;
    {
        for (const ProductDescriptor &desc : product_table()) {
            const size_t i = product_index(desc.type);
            app.luts[i] = make_product_lut_texture(desc.type);
            app.configs[i] = make_product_render_config(desc.type);
        }

        MomentRenderer moment_renderer;
        SmoothMomentRenderer smooth_renderer;
        OverlayRenderer overlay_renderer;
        LegendRenderer legend_renderer;
        MapRenderer map_renderer;

        if (!moment_renderer.initialize()) {
            std::fprintf(stderr, "Failed to initialize moment renderer\n");
            exit_code = 1;
        }

        if (exit_code == 0 && !smooth_renderer.initialize()) {
            std::fprintf(stderr, "Failed to initialize smooth renderer\n");
            exit_code = 1;
        }

        if (exit_code == 0 && !overlay_renderer.initialize(moment_renderer.max_range())) {
            std::fprintf(stderr, "Failed to initialize overlay renderer\n");
            exit_code = 1;
        }

        if (exit_code == 0 && !legend_renderer.initialize()) {
            std::fprintf(stderr, "Failed to initialize legend renderer\n");
            exit_code = 1;
        }

        if (exit_code == 0) {
            if (!map_renderer.initialize()) {
                std::fprintf(stderr, "Failed to initialize map renderer\n");
                exit_code = 1;
            } else {
                // raw layers load once, projection happens when the first
                // volume tells us the site. Missing assets just warn.
                load_polyline_file("assets/maps/states.lines", app.raw_states);
                load_polyline_file("assets/maps/counties.lines", app.raw_counties);
                load_places_file("assets/maps/places.pts", app.raw_places);
            }
        }

        VolumeLoader loader;
        VolumeSet volumes;
        if (exit_code == 0) {
            if (volumes.open(config.level2_path)) {
                view.num_volumes = volumes.size();
                view.requested_volume_idx = 0;
            } else {
                app.load_error = "No Level 2 archives found at " + config.level2_path;
                std::fprintf(stderr, "%s\n", app.load_error.c_str());
            }
        }

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(1.0f);

        const char *autoshot_path = std::getenv("OPENREFL_AUTOSHOT");
        constexpr long kAutoshotFrame = 30;
        long frame_counter = 0;

        // OPENREFL_VIEW="zoom,cx_km,cy_km" starts the view zoomed onto a
        // world point (km east/north of the site). Applied on the first
        // frame once the viewport size is known.
        float env_zoom = 0.0f, env_cx_km = 0.0f, env_cy_km = 0.0f;
        bool view_env_pending = false;
        if (const char *view_env = std::getenv("OPENREFL_VIEW")) {
            view_env_pending = std::sscanf(view_env, "%f,%f,%f",
                                           &env_zoom, &env_cx_km, &env_cy_km) == 3;
        }
        int smooth_uploaded_pi = -1;
        int smooth_uploaded_idx = -1;

        while (exit_code == 0 && !glfwWindowShouldClose(window)) {
            if (std::optional<LoadedVolume> vol = loader.poll()) {
                if (!vol->error.empty()) {
                    volumes.fail_request(vol->path);
                    app.load_error = vol->error;
                    std::fprintf(stderr, "Load failed: %s\n", vol->error.c_str());
                } else {
                    // results from a set replaced mid-flight match nothing
                    // and get dropped
                    volumes.store_result(std::move(*vol));
                }
            }
            if (!app.pending_drop_path.empty()) {
                if (volumes.open(app.pending_drop_path)) {
                    view.num_volumes = volumes.size();
                    view.volume_idx = -1;
                    view.requested_volume_idx = 0;
                    if (view.num_volumes <= 1) {
                        view.playback_mode = ViewState::PlaybackMode::Sweeps;
                    }
                    app.load_error.clear();
                } else {
                    app.load_error = "Nothing loadable at " + app.pending_drop_path;
                }
                app.pending_drop_path.clear();
            }
            volumes.pump(loader,
                         app.site.site_id.empty() ? config.site_id : app.site.site_id,
                         view.requested_volume_idx);
            if (view.requested_volume_idx != volumes.active()) {
                // no-op if the target isn't decoded yet, the pump is
                // fetching it and the jump lands on a later frame
                if (activate_volume(app, volumes, view.requested_volume_idx, window,
                                    moment_renderer, overlay_renderer, map_renderer,
                                    smooth_uploaded_pi, smooth_uploaded_idx)) {
                    volumes.evict();
                }
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            draw_sidebar(window, app, volumes);

            if (loader.busy() && !volumes.decoded(view.requested_volume_idx)) {
                ImGuiIO &io = ImGui::GetIO();
                ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                                        ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowBgAlpha(0.7f);
                ImGui::Begin("##loading", nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoInputs);
                const int dots = 1 + static_cast<int>(glfwGetTime() * 2.0) % 3;
                ImGui::Text("Decoding Level 2 archive%.*s", dots, "...");
                ImGui::End();
            }

            const double current_time = glfwGetTime();
            const bool product_changed = view.requested_product != view.current_product;
            const float playback_sweeps_per_second =
                std::clamp(view.playback_sweeps_per_second,
                           kMinPlaybackSweepsPerSecond, kMaxPlaybackSweepsPerSecond);
            const double playback_interval_seconds =
                1.0 / static_cast<double>(playback_sweeps_per_second);
            if (!product_changed && view.playback_active &&
                current_time - view.last_playback_advance_time >= playback_interval_seconds) {
                if (view.playback_mode == ViewState::PlaybackMode::Time) {
                    // hold until the next volume is decoded so playback
                    // doesn't skip
                    if (view.num_volumes > 1) {
                        const int next = (view.requested_volume_idx + 1) % view.num_volumes;
                        if (volumes.decoded(next)) {
                            view.requested_volume_idx = next;
                            view.last_playback_advance_time = current_time;
                        }
                    }
                } else if (view.num_scans > 1) {
                    view.request_scan_delta_wrapped(1);
                    view.last_playback_advance_time = current_time;
                }
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
            const bool have_data = !app.products[pi].scans.empty();
            ProductRenderConfig effective = app.configs[pi];
            const bool srv_active =
                view.current_product == rsl::ProductType::VELOCITY && app.srv_enabled;
            if (view.current_product == rsl::ProductType::VELOCITY && have_data) {
                const float nyq = app.products[pi].scans[static_cast<size_t>(view.scan_idx)].nyquist_vel;
                if (nyq > 0.0f) {
                    effective.min_value = -nyq;
                    effective.max_value = nyq;
                }
                if (srv_active) {
                    app.storm_motion_uv(effective.storm_u, effective.storm_v);
                }
            }

            // smoothed path lazily builds/uploads the polar texture for the
            // active sweep. Falls back to the crisp path when the sweep
            // can't be gridded.
            bool smoothing_active = app.smoothing_enabled && have_data;
            if (smoothing_active) {
                if (smooth_uploaded_pi != static_cast<int>(pi) ||
                    smooth_uploaded_idx != view.scan_idx) {
                    auto &pcache = app.polar_caches[pi];
                    const size_t idx = static_cast<size_t>(view.scan_idx);
                    if (!pcache[idx]) {
                        pcache[idx] = build_scan_polar_texture(app.products[pi].scans[idx]);
                    }
                    smooth_renderer.upload_scan(*pcache[idx]);
                    smooth_uploaded_pi = static_cast<int>(pi);
                    smooth_uploaded_idx = view.scan_idx;
                }
                smoothing_active = smooth_renderer.has_scan();
            }

            if (view_env_pending && moment_renderer.max_range() > 0.0f) {
                view.zoom = env_zoom;
                const ViewProjection base =
                    make_view_projection(moment_renderer.max_range(), radar_width_fb, fbh,
                                         view.zoom, 0.0f, 0.0f);
                view.offset_x = -env_cx_km * 1000.0f * base.scale_x;
                view.offset_y = -env_cy_km * 1000.0f * base.scale_y;
                view_env_pending = false;
            }

            const ViewProjection projection =
                make_view_projection(moment_renderer.max_range(), radar_width_fb, fbh,
                                     view.zoom, view.offset_x, view.offset_y);
            if (smoothing_active) {
                smooth_renderer.draw(projection, app.luts[pi], effective);
            } else {
                moment_renderer.draw(projection, app.luts[pi], effective);
            }
            map_renderer.draw(projection, view.zoom);
            overlay_renderer.draw(projection);
            if (have_data) {
                legend_renderer.draw(radar_width_fb, fbh, app.luts[pi], effective);
            }

            const float sidebar_width_win = sidebar_width_for_window(ww);
            const ViewportRect viewport_win{
                sidebar_width_win, 0.0f,
                std::max(1.0f, static_cast<float>(ww) - sidebar_width_win),
                static_cast<float>(wh)};
            draw_place_labels(app.projected_places, projection, view.zoom, viewport_win);
            draw_ring_and_cardinal_labels(projection, moment_renderer.max_range(), viewport_win);

            const CursorReadout readout =
                compute_cursor_readout(app, projection, viewport_win, effective);
            if (have_data) {
                draw_inspector_overlay(readout, effective.unit_label, viewport_win);
            }

            // legend bar geometry mirrors LegendRenderer::draw, converted
            // from framebuffer pixels (origin bottom-left) to window coords
            if (have_data) {
                const float scale_y = (wh > 0) ? static_cast<float>(fbh) / static_cast<float>(wh) : 1.0f;
                const float bar_h_fb = std::max(80.0f, static_cast<float>(fbh) * 0.6f);
                const float bar_x_win =
                    (static_cast<float>(sidebar_width_fb + radar_width_fb) - 16.0f - 18.0f) /
                    framebuffer_scale_x;
                const float bar_y_win = ((static_cast<float>(fbh) - bar_h_fb) * 0.5f) / scale_y;
                const char *legend_label =
                    srv_active ? "Storm-Rel Velocity" : product_label(view.current_product);
                draw_legend_annotations(legend_label, effective.unit_label,
                                        effective.min_value, effective.max_value,
                                        effective.tick_period,
                                        bar_x_win, bar_y_win, 18.0f / framebuffer_scale_x,
                                        bar_h_fb / scale_y);
            }

            glViewport(0, 0, fbw, fbh);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            if (app.screenshot_requested) {
                app.screenshot_requested = false;
                save_screenshot_png(fbw, fbh);
            }
            // OPENREFL_AUTOSHOT=<path> grabs one frame shortly after the
            // first volume renders, handy for quick visual checks
            if (have_data) {
                if (autoshot_path && frame_counter == kAutoshotFrame) {
                    save_screenshot_png_to(fbw, fbh, autoshot_path);
                    std::printf("Saved autoshot: %s\n", autoshot_path);
                    std::fflush(stdout);
                }
                ++frame_counter;
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    shutdown_imgui();
    glfwDestroyWindow(window);
    glfwTerminate();
    return exit_code;
}
