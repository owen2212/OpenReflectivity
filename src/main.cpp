#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <string>
#include <algorithm>
#include <numeric>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "rsl/rsl_wrapper.hpp"
#include "gl/buffer.hpp"
#include "gl/vertex_array.hpp"
#include "gl/shader.hpp"


int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Boilerplate Test", nullptr, nullptr);
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

    // RSL wrapper
    const std::string level2_path = "examples/KTLX20130520_000122_V06";
    const std::string site_id = "KTLX";

    rsl::RadarData radar_data(level2_path, site_id);
    rsl::Product ref = radar_data.get_product(rsl::REFLECTIVITY);

    // for(float &f : ref.scans.at(0).radials.at(0).gates){
    //     std::printf("%f\n", f);
    // }

    struct GateData{
        float gate;
        int gate_idx;
        int radial_idx;
    };

    struct RadialMetaData{
        float azimuth;
        float range_bin1;
        float gate_size;
    };

    std::vector<GateData> initial_gate_layout; // build the layout
    std::vector<RadialMetaData> meta_data_layout;
    std::vector<float> azimuths_deg;
    float max_range = 0.0f;
    int radial_num = 0;
    for(rsl::Radial &r : ref.scans.at(0).radials){
        RadialMetaData m;
        m.azimuth = r.azimuth;
        m.gate_size = r.gate_size;
        m.range_bin1 = r.range_bin1;
        meta_data_layout.push_back(m);
        azimuths_deg.push_back(r.azimuth);
        if (!r.gates.empty()) {
            float radial_max = r.range_bin1 + r.gate_size * static_cast<float>(r.gates.size() - 1);
            if (radial_max > max_range) max_range = radial_max;
        } else if (r.range_bin1 > max_range) {
            max_range = r.range_bin1;
        }
        int gate_idx = 0;
        for(float &f : r.gates){
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
        std::fprintf(stderr, "No gate data to draw (gates=%zu, radials=%zu)\n", gate_count, radial_count);
    }

    {
        VertexArray vao(true);
        Buffer vbo(Buffer::Target::Array);
        Buffer quad_vbo(Buffer::Target::Array);
        Buffer meta_buffer(Buffer::Target::Array);
        GLuint meta_tex = 0;

        vao.bind();
        vbo.bind();

        vbo.set_data(gate_arr, sizeof(GateData) * gate_count, Buffer::Usage::StaticDraw);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(GateData), (void*)offsetof(GateData, gate)); // gate val
        glVertexAttribIPointer(2, 1, GL_INT, sizeof(GateData), (void*)offsetof(GateData, gate_idx)); // gate index
        glVertexAttribIPointer(3, 1, GL_INT, sizeof(GateData), (void*)offsetof(GateData, radial_idx)); // radial index
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);

        constexpr float quad_vertices[] = {
            -0.5f, -0.5f,
             0.5f, -0.5f,
             0.5f,  0.5f,
            -0.5f, -0.5f,
             0.5f,  0.5f,
            -0.5f,  0.5f,
        };

        quad_vbo.bind();
        quad_vbo.set_data(quad_vertices, sizeof(quad_vertices), Buffer::Usage::StaticDraw);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0); // quad position
        glEnableVertexAttribArray(0);

        std::vector<float> delta_az_rad(radial_count, 0.0f);
        std::vector<float> azimuth_center_deg(radial_count, 0.0f);
        if (!azimuths_deg.empty()) {
            std::vector<size_t> order(radial_count);
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
                return azimuths_deg[a] < azimuths_deg[b];
            });

            for (size_t oi = 0; oi < order.size(); ++oi) {
                const size_t idx = order[oi];
                const size_t next_idx = order[(oi + 1) % order.size()];
                float curr = azimuths_deg[idx];
                float next = azimuths_deg[next_idx];
                if (oi + 1 == order.size()) {
                    next += 360.0f;
                }
                float d = next - curr;
                if (d < 0.0f) d += 360.0f;
                delta_az_rad[idx] = d * 0.01745329252f;
                float center = curr + 0.5f * d;
                if (center >= 360.0f) center -= 360.0f;
                azimuth_center_deg[idx] = center;
            }
        }

        std::vector<float> meta_packed;
        meta_packed.reserve(meta_data_layout.size() * 4);
        for (size_t i = 0; i < meta_data_layout.size(); ++i) {
            const RadialMetaData &m = meta_data_layout[i];
            meta_packed.push_back(azimuth_center_deg[i]);
            meta_packed.push_back(m.range_bin1);
            meta_packed.push_back(m.gate_size);
            meta_packed.push_back(delta_az_rad[i]);
        }

        meta_buffer.bind();
        meta_buffer.set_data(meta_packed.data(), sizeof(float) * meta_packed.size(), Buffer::Usage::StaticDraw);
        glGenTextures(1, &meta_tex);
        glBindTexture(GL_TEXTURE_BUFFER, meta_tex);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, meta_buffer.id());

        Shader shader;
        if (!shader.load_files("shaders/ref.vert", "shaders/ref.frag")) {
            std::fprintf(stderr, "Failed to load shaders\n");
            if (meta_tex) glDeleteTextures(1, &meta_tex);
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }

        shader.use();
        shader.set_int("u_radial_meta", 0);
        const int scale_loc = glGetUniformLocation((GLuint)shader.id(), "u_view_scale");
        const int offset_loc = glGetUniformLocation((GLuint)shader.id(), "u_view_offset");
        if (offset_loc >= 0) glUniform2f(offset_loc, 0.0f, 0.0f);

        while (!glfwWindowShouldClose(window)) {
            int fbw = 0, fbh = 0;
            glfwGetFramebufferSize(window, &fbw, &fbh);
            glViewport(0, 0, fbw, fbh);
            if (scale_loc >= 0) {
                const float aspect = (fbh > 0) ? (static_cast<float>(fbw) / static_cast<float>(fbh)) : 1.0f;
                float sx = 1.0f;
                float sy = 1.0f;
                if (max_range > 0.0f) {
                    if (aspect >= 1.0f) {
                        sx = 1.0f / max_range;
                        sy = aspect / max_range;
                    } else {
                        sx = 1.0f / (max_range * aspect);
                        sy = 1.0f / max_range;
                    }
                }
                glUniform2f(scale_loc, sx, sy);
            }
            glClearColor(0.08f, 0.10f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            if (gate_count > 0 && radial_count > 0) {
                shader.use();
                vao.bind();
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_BUFFER, meta_tex);
                glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(gate_count));
            }

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        if (meta_tex) {
            glDeleteTextures(1, &meta_tex);
        }
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
