#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cmath>
#include <string>
#include <algorithm>
#include <array>
#include <numeric>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "rsl/rsl_wrapper.hpp"
#include "gl/buffer.hpp"
#include "gl/vertex_array.hpp"
#include "gl/shader.hpp"


namespace {

constexpr float kMinDbz = 5.0f;
constexpr float kMaxDbz = 75.0f;
constexpr int kLutSize = 256;

struct ColorStop { float dbz, r, g, b; };

constexpr std::array<ColorStop, 11> kColorStops = {{
    {  5.0f, 0.20f, 0.55f, 0.80f},
    { 10.0f, 0.10f, 0.75f, 0.85f},
    { 15.0f, 0.10f, 0.85f, 0.55f},
    { 20.0f, 0.10f, 0.80f, 0.20f},
    { 25.0f, 0.30f, 0.85f, 0.10f},
    { 30.0f, 0.95f, 0.95f, 0.15f},
    { 35.0f, 0.98f, 0.75f, 0.10f},
    { 40.0f, 0.98f, 0.50f, 0.10f},
    { 50.0f, 0.95f, 0.10f, 0.10f},
    { 60.0f, 0.85f, 0.10f, 0.85f},
    { 75.0f, 0.95f, 0.95f, 0.95f},
}};

std::vector<unsigned char> build_dbz_lut() {
    std::vector<unsigned char> lut(static_cast<size_t>(kLutSize) * 3);
    for (int i = 0; i < kLutSize; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kLutSize - 1);
        float dbz = kMinDbz + t * (kMaxDbz - kMinDbz);
        size_t hi = 0;
        while (hi < kColorStops.size() && kColorStops[hi].dbz < dbz) ++hi;
        float r, g, b;
        if (hi == 0) {
            r = kColorStops[0].r; g = kColorStops[0].g; b = kColorStops[0].b;
        } else if (hi >= kColorStops.size()) {
            const auto &s = kColorStops.back();
            r = s.r; g = s.g; b = s.b;
        } else {
            const auto &lo = kColorStops[hi - 1];
            const auto &up = kColorStops[hi];
            float seg = (dbz - lo.dbz) / (up.dbz - lo.dbz);
            r = lo.r + seg * (up.r - lo.r);
            g = lo.g + seg * (up.g - lo.g);
            b = lo.b + seg * (up.b - lo.b);
        }
        lut[i * 3 + 0] = static_cast<unsigned char>(r * 255.0f + 0.5f);
        lut[i * 3 + 1] = static_cast<unsigned char>(g * 255.0f + 0.5f);
        lut[i * 3 + 2] = static_cast<unsigned char>(b * 255.0f + 0.5f);
    }
    return lut;
}

struct GateData {
    float gate;
    int gate_idx;
    int radial_idx;
};

struct ScanGpuData {
    std::vector<GateData> gates;
    std::vector<float> meta_packed; // 4 floats per radial: az_start_rad, range_bin1, gate_size, delta_az_rad
    float max_range = 0.0f;
    size_t radial_count = 0;
};

ScanGpuData build_scan_gpu_data(const rsl::Scan &scan) {
    ScanGpuData out;
    const size_t radial_count = scan.radials.size();
    out.radial_count = radial_count;
    if (radial_count == 0) return out;

    std::vector<float> azimuths_deg;
    std::vector<float> range_bin1_per_radial;
    std::vector<float> gate_size_per_radial;
    azimuths_deg.reserve(radial_count);
    range_bin1_per_radial.reserve(radial_count);
    gate_size_per_radial.reserve(radial_count);

    for (size_t i = 0; i < radial_count; ++i) {
        const rsl::Radial &r = scan.radials[i];
        azimuths_deg.push_back(r.azimuth);
        range_bin1_per_radial.push_back(r.range_bin1);
        gate_size_per_radial.push_back(r.gate_size);
        if (!r.gates.empty()) {
            float radial_max = r.range_bin1 + r.gate_size * static_cast<float>(r.gates.size());
            if (radial_max > out.max_range) out.max_range = radial_max;
        } else if (r.range_bin1 > out.max_range) {
            out.max_range = r.range_bin1;
        }
        const int radial_idx = static_cast<int>(i);
        for (size_t j = 0; j < r.gates.size(); ++j) {
            GateData d;
            d.gate = r.gates[j];
            d.gate_idx = static_cast<int>(j);
            d.radial_idx = radial_idx;
            out.gates.push_back(d);
        }
    }

    std::vector<float> delta_az_rad(radial_count, 0.0f);
    std::vector<float> az_start_rad(radial_count, 0.0f);

    std::vector<size_t> order(radial_count);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return azimuths_deg[a] < azimuths_deg[b];
    });

    constexpr float kDeg2Rad = 0.01745329252f;
    for (size_t oi = 0; oi < order.size(); ++oi) {
        const size_t idx = order[oi];
        const size_t next_idx = order[(oi + 1) % order.size()];
        float curr = azimuths_deg[idx];
        float next = azimuths_deg[next_idx];
        if (oi + 1 == order.size()) next += 360.0f;
        float d = next - curr;
        if (d < 0.0f) d += 360.0f;
        delta_az_rad[idx] = d * kDeg2Rad;
        float start = curr - 0.5f * d;
        az_start_rad[idx] = start * kDeg2Rad;
    }

    out.meta_packed.reserve(radial_count * 4);
    for (size_t i = 0; i < radial_count; ++i) {
        out.meta_packed.push_back(az_start_rad[i]);
        out.meta_packed.push_back(range_bin1_per_radial[i]);
        out.meta_packed.push_back(gate_size_per_radial[i]);
        out.meta_packed.push_back(delta_az_rad[i]);
    }
    return out;
}

struct OverlayGeometry {
    std::vector<float> verts;          // x,y world meters
    std::vector<int> ring_offsets;     // first vertex of each ring
    std::vector<int> ring_counts;      // vertex count per ring
    std::vector<float> ring_ranges_km; // for printout / future labels
    int marker_offset = 0;
    int marker_count = 0;
};

OverlayGeometry build_overlay(float max_range_m) {
    OverlayGeometry o;
    constexpr int kSegments = 192;
    constexpr float kTwoPi = 6.28318530718f;

    const std::array<float, 5> ring_km = {25.0f, 50.0f, 100.0f, 150.0f, 200.0f};
    for (float rk : ring_km) {
        const float r_m = rk * 1000.0f;
        if (r_m > max_range_m * 1.05f) continue;
        const int start = static_cast<int>(o.verts.size() / 2);
        for (int s = 0; s < kSegments; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(kSegments);
            float a = t * kTwoPi;
            o.verts.push_back(std::cos(a) * r_m);
            o.verts.push_back(std::sin(a) * r_m);
        }
        o.ring_offsets.push_back(start);
        o.ring_counts.push_back(kSegments);
        o.ring_ranges_km.push_back(rk);
    }

    // Site marker: a "+" cross of total span ~10 km, with a small gap at center.
    const float arm = 5000.0f;
    const float gap = 800.0f;
    o.marker_offset = static_cast<int>(o.verts.size() / 2);
    const float marker_pts[] = {
        // horizontal
         gap,  0.0f,   arm, 0.0f,
        -gap,  0.0f,  -arm, 0.0f,
        // vertical
         0.0f,  gap,   0.0f,  arm,
         0.0f, -gap,   0.0f, -arm,
    };
    for (float v : marker_pts) o.verts.push_back(v);
    o.marker_count = static_cast<int>(sizeof(marker_pts) / sizeof(float) / 2);

    return o;
}

struct View {
    float offset_x = 0.0f; // in NDC
    float offset_y = 0.0f;
    float zoom = 1.0f;
    bool dragging = false;
    double last_cursor_x = 0.0;
    double last_cursor_y = 0.0;

    int scan_idx = 0;
    int requested_scan_idx = 0;
    int num_scans = 1;
};

void cursor_pos_callback(GLFWwindow *win, double xpos, double ypos) {
    View *v = static_cast<View*>(glfwGetWindowUserPointer(win));
    if (!v) return;
    if (v->dragging) {
        int ww = 0, wh = 0;
        glfwGetWindowSize(win, &ww, &wh);
        double dx = xpos - v->last_cursor_x;
        double dy = ypos - v->last_cursor_y;
        if (ww > 0) v->offset_x += static_cast<float>(2.0 * dx / ww);
        if (wh > 0) v->offset_y -= static_cast<float>(2.0 * dy / wh);
    }
    v->last_cursor_x = xpos;
    v->last_cursor_y = ypos;
}

void mouse_button_callback(GLFWwindow *win, int button, int action, int /*mods*/) {
    View *v = static_cast<View*>(glfwGetWindowUserPointer(win));
    if (!v) return;
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    if (action == GLFW_PRESS) {
        v->dragging = true;
        glfwGetCursorPos(win, &v->last_cursor_x, &v->last_cursor_y);
    } else if (action == GLFW_RELEASE) {
        v->dragging = false;
    }
}

void scroll_callback(GLFWwindow *win, double /*xoff*/, double yoff) {
    View *v = static_cast<View*>(glfwGetWindowUserPointer(win));
    if (!v) return;
    int ww = 0, wh = 0;
    glfwGetWindowSize(win, &ww, &wh);
    double cx = 0.0, cy = 0.0;
    glfwGetCursorPos(win, &cx, &cy);
    float ncx = (ww > 0) ? static_cast<float>(2.0 * cx / ww - 1.0) : 0.0f;
    float ncy = (wh > 0) ? static_cast<float>(1.0 - 2.0 * cy / wh) : 0.0f;
    float new_zoom = v->zoom * std::pow(1.1f, static_cast<float>(yoff));
    if (new_zoom < 0.1f) new_zoom = 0.1f;
    if (new_zoom > 50.0f) new_zoom = 50.0f;
    float f = new_zoom / v->zoom;
    v->offset_x = ncx * (1.0f - f) + v->offset_x * f;
    v->offset_y = ncy * (1.0f - f) + v->offset_y * f;
    v->zoom = new_zoom;
}

void key_callback(GLFWwindow *win, int key, int /*sc*/, int action, int /*mods*/) {
    View *v = static_cast<View*>(glfwGetWindowUserPointer(win));
    if (!v) return;
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (action == GLFW_PRESS && key == GLFW_KEY_R) {
        v->offset_x = 0.0f;
        v->offset_y = 0.0f;
        v->zoom = 1.0f;
    } else if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
    } else if (key == GLFW_KEY_LEFT_BRACKET) {
        int idx = v->requested_scan_idx - 1;
        if (idx < 0) idx = 0;
        v->requested_scan_idx = idx;
    } else if (key == GLFW_KEY_RIGHT_BRACKET) {
        int idx = v->requested_scan_idx + 1;
        if (idx >= v->num_scans) idx = v->num_scans - 1;
        v->requested_scan_idx = idx;
    }
}

} // namespace


int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1024, 768, "OpenReflectivity", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "Failed to initialize GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::printf("OpenGL Vendor  : %s\n", glGetString(GL_VENDOR));
    std::printf("OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
    std::printf("OpenGL Version : %s\n", glGetString(GL_VERSION));

    const std::string level2_path = "examples/KTLX20130520_000122_V06";
    const std::string site_id = "KTLX";

    rsl::RadarData radar_data(level2_path, site_id);
    rsl::Product ref = radar_data.get_product(rsl::REFLECTIVITY);

    if (ref.scans.empty()) {
        std::fprintf(stderr, "No scans in product\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    View view;
    view.num_scans = static_cast<int>(ref.scans.size());
    glfwSetWindowUserPointer(window, &view);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    std::printf("Loaded %d sweeps. Use [ and ] to switch elevation, R to reset view, Esc to quit.\n",
                view.num_scans);
    std::printf("Active sweep: 0 (elevation %.2f deg)\n", ref.scans[0].elevation);

    {
        VertexArray vao(true);
        Buffer vbo(Buffer::Target::Array);
        Buffer quad_vbo(Buffer::Target::Array);
        Buffer meta_buffer(Buffer::Target::Array);
        GLuint meta_tex = 0;
        GLuint dbz_tex = 0;

        vao.bind();
        vbo.bind();
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(GateData),
                              (void*)offsetof(GateData, gate));
        glVertexAttribIPointer(2, 1, GL_INT, sizeof(GateData),
                               (void*)offsetof(GateData, gate_idx));
        glVertexAttribIPointer(3, 1, GL_INT, sizeof(GateData),
                               (void*)offsetof(GateData, radial_idx));
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);

        // Unit quad in [0,1] x [0,1] parametric space.
        constexpr float quad_vertices[] = {
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 1.0f,
            0.0f, 1.0f,
        };

        quad_vbo.bind();
        quad_vbo.set_data(quad_vertices, sizeof(quad_vertices), Buffer::Usage::StaticDraw);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Initial scan upload.
        ScanGpuData scan_data = build_scan_gpu_data(ref.scans[view.scan_idx]);
        size_t gate_count = scan_data.gates.size();
        size_t radial_count = scan_data.radial_count;
        float max_range = scan_data.max_range;

        vbo.bind();
        vbo.set_data(scan_data.gates.data(),
                     sizeof(GateData) * gate_count,
                     Buffer::Usage::StaticDraw);

        meta_buffer.bind();
        meta_buffer.set_data(scan_data.meta_packed.data(),
                             sizeof(float) * scan_data.meta_packed.size(),
                             Buffer::Usage::StaticDraw);
        glGenTextures(1, &meta_tex);
        glBindTexture(GL_TEXTURE_BUFFER, meta_tex);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, meta_buffer.id());

        // dBZ color LUT.
        {
            std::vector<unsigned char> lut = build_dbz_lut();
            glGenTextures(1, &dbz_tex);
            glBindTexture(GL_TEXTURE_1D, dbz_tex);
            glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8, kLutSize, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, lut.data());
            glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        }

        Shader shader;
        if (!shader.load_files("shaders/ref.vert", "shaders/ref.frag")) {
            std::fprintf(stderr, "Failed to load reflectivity shaders\n");
            if (meta_tex) glDeleteTextures(1, &meta_tex);
            if (dbz_tex) glDeleteTextures(1, &dbz_tex);
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }
        shader.use();
        shader.set_int("u_radial_meta", 0);
        shader.set_int("u_dbz_lut", 1);
        shader.set_float("u_min_dbz", kMinDbz);
        shader.set_float("u_max_dbz", kMaxDbz);
        const int scale_loc  = glGetUniformLocation((GLuint)shader.id(), "u_view_scale");
        const int offset_loc = glGetUniformLocation((GLuint)shader.id(), "u_view_offset");

        // Overlay (rings + site marker) resources.
        OverlayGeometry overlay = build_overlay(max_range);
        VertexArray overlay_vao(true);
        Buffer overlay_vbo(Buffer::Target::Array);
        overlay_vao.bind();
        overlay_vbo.bind();
        overlay_vbo.set_data(overlay.verts.data(),
                             sizeof(float) * overlay.verts.size(),
                             Buffer::Usage::StaticDraw);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        Shader overlay_shader;
        if (!overlay_shader.load_files("shaders/overlay.vert", "shaders/overlay.frag")) {
            std::fprintf(stderr, "Failed to load overlay shaders\n");
            if (meta_tex) glDeleteTextures(1, &meta_tex);
            if (dbz_tex) glDeleteTextures(1, &dbz_tex);
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }
        const int ovl_scale_loc  = glGetUniformLocation((GLuint)overlay_shader.id(), "u_view_scale");
        const int ovl_offset_loc = glGetUniformLocation((GLuint)overlay_shader.id(), "u_view_offset");
        const int ovl_color_loc  = glGetUniformLocation((GLuint)overlay_shader.id(), "u_color");

        // Legend resources.
        VertexArray legend_vao(true);
        Buffer legend_vbo(Buffer::Target::Array);
        legend_vao.bind();
        legend_vbo.bind();
        legend_vbo.set_data(quad_vertices, sizeof(quad_vertices), Buffer::Usage::StaticDraw);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        Shader legend_shader;
        if (!legend_shader.load_files("shaders/legend.vert", "shaders/legend.frag")) {
            std::fprintf(stderr, "Failed to load legend shaders\n");
            if (meta_tex) glDeleteTextures(1, &meta_tex);
            if (dbz_tex) glDeleteTextures(1, &dbz_tex);
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }
        legend_shader.use();
        legend_shader.set_int("u_dbz_lut", 1);
        legend_shader.set_float("u_min_dbz", kMinDbz);
        legend_shader.set_float("u_max_dbz", kMaxDbz);
        const int leg_rect_loc   = glGetUniformLocation((GLuint)legend_shader.id(), "u_pixel_rect");
        const int leg_screen_loc = glGetUniformLocation((GLuint)legend_shader.id(), "u_screen_size");
        const int leg_psize_loc  = glGetUniformLocation((GLuint)legend_shader.id(), "u_pixel_size");

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(1.0f);

        while (!glfwWindowShouldClose(window)) {
            // Handle elevation switch.
            if (view.requested_scan_idx != view.scan_idx) {
                int new_idx = view.requested_scan_idx;
                if (new_idx < 0) new_idx = 0;
                if (new_idx >= view.num_scans) new_idx = view.num_scans - 1;
                view.scan_idx = new_idx;
                view.requested_scan_idx = new_idx;

                scan_data = build_scan_gpu_data(ref.scans[new_idx]);
                gate_count = scan_data.gates.size();
                radial_count = scan_data.radial_count;
                max_range = scan_data.max_range;

                vbo.bind();
                vbo.set_data(scan_data.gates.data(),
                             sizeof(GateData) * gate_count,
                             Buffer::Usage::StaticDraw);
                meta_buffer.bind();
                meta_buffer.set_data(scan_data.meta_packed.data(),
                                     sizeof(float) * scan_data.meta_packed.size(),
                                     Buffer::Usage::StaticDraw);
                glBindTexture(GL_TEXTURE_BUFFER, meta_tex);
                glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, meta_buffer.id());

                std::printf("Active sweep: %d (elevation %.2f deg)\n",
                            new_idx, ref.scans[new_idx].elevation);
                std::fflush(stdout);
            }

            int fbw = 0, fbh = 0;
            glfwGetFramebufferSize(window, &fbw, &fbh);
            glViewport(0, 0, fbw, fbh);

            glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            float base_sx = 1.0f, base_sy = 1.0f;
            if (max_range > 0.0f && fbw > 0 && fbh > 0) {
                const float aspect = static_cast<float>(fbw) / static_cast<float>(fbh);
                if (aspect >= 1.0f) {
                    base_sx = 1.0f / max_range;
                    base_sy = aspect / max_range;
                } else {
                    base_sx = 1.0f / (max_range * aspect);
                    base_sy = 1.0f / max_range;
                }
            }
            const float sx = base_sx * view.zoom;
            const float sy = base_sy * view.zoom;

            // Reflectivity pass.
            if (gate_count > 0 && radial_count > 0) {
                shader.use();
                if (scale_loc >= 0)  glUniform2f(scale_loc, sx, sy);
                if (offset_loc >= 0) glUniform2f(offset_loc, view.offset_x, view.offset_y);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_BUFFER, meta_tex);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_1D, dbz_tex);
                vao.bind();
                glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(gate_count));
            }

            // Overlay pass: range rings + site marker.
            {
                overlay_shader.use();
                if (ovl_scale_loc >= 0)  glUniform2f(ovl_scale_loc, sx, sy);
                if (ovl_offset_loc >= 0) glUniform2f(ovl_offset_loc, view.offset_x, view.offset_y);
                overlay_vao.bind();

                // Range rings: subtle grey.
                if (ovl_color_loc >= 0)
                    glUniform4f(ovl_color_loc, 0.55f, 0.60f, 0.68f, 0.55f);
                for (size_t i = 0; i < overlay.ring_offsets.size(); ++i) {
                    glDrawArrays(GL_LINE_LOOP, overlay.ring_offsets[i], overlay.ring_counts[i]);
                }

                // Site marker: bright white.
                if (overlay.marker_count > 0) {
                    if (ovl_color_loc >= 0)
                        glUniform4f(ovl_color_loc, 1.0f, 1.0f, 1.0f, 0.95f);
                    glDrawArrays(GL_LINES, overlay.marker_offset, overlay.marker_count);
                }
            }

            // Legend pass.
            if (fbw > 0 && fbh > 0) {
                const float bar_w = 18.0f;
                const float bar_margin = 16.0f;
                const float bar_h = std::max(80.0f, static_cast<float>(fbh) * 0.6f);
                const float bar_x = static_cast<float>(fbw) - bar_margin - bar_w;
                const float bar_y = (static_cast<float>(fbh) - bar_h) * 0.5f;

                legend_shader.use();
                if (leg_rect_loc >= 0)
                    glUniform4f(leg_rect_loc, bar_x, bar_y, bar_w, bar_h);
                if (leg_screen_loc >= 0)
                    glUniform2f(leg_screen_loc, static_cast<float>(fbw), static_cast<float>(fbh));
                if (leg_psize_loc >= 0)
                    glUniform2f(leg_psize_loc, bar_w, bar_h);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_1D, dbz_tex);
                legend_vao.bind();
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        if (meta_tex) glDeleteTextures(1, &meta_tex);
        if (dbz_tex)  glDeleteTextures(1, &dbz_tex);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
