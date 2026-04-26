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

struct View {
    float offset_x = 0.0f; // in NDC
    float offset_y = 0.0f;
    float zoom = 1.0f;
    bool dragging = false;
    double last_cursor_x = 0.0;
    double last_cursor_y = 0.0;
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
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_R) {
        v->offset_x = 0.0f;
        v->offset_y = 0.0f;
        v->zoom = 1.0f;
    } else if (key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
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

    struct GateData {
        float gate;
        int gate_idx;
        int radial_idx;
    };

    struct RadialMetaData {
        float azimuth;
        float range_bin1;
        float gate_size;
    };

    std::vector<GateData> initial_gate_layout;
    std::vector<RadialMetaData> meta_data_layout;
    std::vector<float> azimuths_deg;
    float max_range = 0.0f;
    int radial_num = 0;
    for (rsl::Radial &r : ref.scans.at(0).radials) {
        RadialMetaData m;
        m.azimuth = r.azimuth;
        m.gate_size = r.gate_size;
        m.range_bin1 = r.range_bin1;
        meta_data_layout.push_back(m);
        azimuths_deg.push_back(r.azimuth);
        if (!r.gates.empty()) {
            float radial_max = r.range_bin1 + r.gate_size * static_cast<float>(r.gates.size());
            if (radial_max > max_range) max_range = radial_max;
        } else if (r.range_bin1 > max_range) {
            max_range = r.range_bin1;
        }
        int gate_idx = 0;
        for (float &f : r.gates) {
            GateData d;
            d.radial_idx = radial_num;
            d.gate = f;
            d.gate_idx = gate_idx;
            initial_gate_layout.push_back(d);
            gate_idx++;
        }
        radial_num++;
    }

    GateData *gate_arr = initial_gate_layout.data();
    const size_t gate_count = initial_gate_layout.size();
    const size_t radial_count = meta_data_layout.size();
    if (gate_count == 0 || radial_count == 0) {
        std::fprintf(stderr, "No gate data to draw (gates=%zu, radials=%zu)\n",
                     gate_count, radial_count);
    }

    View view;
    glfwSetWindowUserPointer(window, &view);
    glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    {
        VertexArray vao(true);
        Buffer vbo(Buffer::Target::Array);
        Buffer quad_vbo(Buffer::Target::Array);
        Buffer meta_buffer(Buffer::Target::Array);
        GLuint meta_tex = 0;
        GLuint dbz_tex = 0;

        vao.bind();
        vbo.bind();

        vbo.set_data(gate_arr, sizeof(GateData) * gate_count, Buffer::Usage::StaticDraw);
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

        // Compute az_start (radians) and delta_az (radians) per radial.
        std::vector<float> delta_az_rad(radial_count, 0.0f);
        std::vector<float> az_start_rad(radial_count, 0.0f);
        if (!azimuths_deg.empty()) {
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
        }

        std::vector<float> meta_packed;
        meta_packed.reserve(meta_data_layout.size() * 4);
        for (size_t i = 0; i < meta_data_layout.size(); ++i) {
            const RadialMetaData &m = meta_data_layout[i];
            meta_packed.push_back(az_start_rad[i]);
            meta_packed.push_back(m.range_bin1);
            meta_packed.push_back(m.gate_size);
            meta_packed.push_back(delta_az_rad[i]);
        }

        meta_buffer.bind();
        meta_buffer.set_data(meta_packed.data(),
                             sizeof(float) * meta_packed.size(),
                             Buffer::Usage::StaticDraw);
        glGenTextures(1, &meta_tex);
        glBindTexture(GL_TEXTURE_BUFFER, meta_tex);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, meta_buffer.id());

        // Build & upload dBZ color LUT as a 1D texture.
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

        while (!glfwWindowShouldClose(window)) {
            int fbw = 0, fbh = 0;
            glfwGetFramebufferSize(window, &fbw, &fbh);
            glViewport(0, 0, fbw, fbh);

            glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // Reflectivity pass.
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
