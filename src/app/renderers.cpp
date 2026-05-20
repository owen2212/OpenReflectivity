#include "app/renderers.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include <glad/glad.h>

namespace {

constexpr float kQuadVertices[] = {
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
};

} // namespace

ViewProjection make_view_projection(float max_range, int framebuffer_width, int framebuffer_height,
                                    float zoom, float offset_x, float offset_y) {
    ViewProjection view;
    if (max_range > 0.0f && framebuffer_width > 0 && framebuffer_height > 0) {
        const float aspect = static_cast<float>(framebuffer_width) / static_cast<float>(framebuffer_height);
        if (aspect >= 1.0f) {
            view.scale_x = 1.0f / max_range;
            view.scale_y = aspect / max_range;
        } else {
            view.scale_x = 1.0f / (max_range * aspect);
            view.scale_y = 1.0f / max_range;
        }
    }
    view.scale_x *= zoom;
    view.scale_y *= zoom;
    view.offset_x = offset_x;
    view.offset_y = offset_y;
    return view;
}

bool MomentRenderer::initialize() {
    vao_.create();
    vao_.bind();

    gate_buffer_.bind();
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(GateData),
                          reinterpret_cast<void*>(offsetof(GateData, gate)));
    glVertexAttribIPointer(2, 1, GL_INT, sizeof(GateData),
                           reinterpret_cast<void*>(offsetof(GateData, gate_idx)));
    glVertexAttribIPointer(3, 1, GL_INT, sizeof(GateData),
                           reinterpret_cast<void*>(offsetof(GateData, radial_idx)));
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(1, 1);
    glVertexAttribDivisor(2, 1);
    glVertexAttribDivisor(3, 1);

    quad_buffer_.bind();
    quad_buffer_.set_data(kQuadVertices, sizeof(kQuadVertices), Buffer::Usage::StaticDraw);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    if (!shader_.load_files("shaders/ref.vert", "shaders/ref.frag")) {
        return false;
    }
    shader_.use();
    shader_.set_int("u_radial_meta", 0);
    shader_.set_int("u_value_lut", 1);
    return true;
}

void MomentRenderer::upload_scan(const ScanGpuData &scan_data) {
    gate_count_ = scan_data.gates.size();
    radial_count_ = scan_data.radial_count;
    max_range_ = scan_data.max_range;

    const size_t gate_bytes = sizeof(GateData) * scan_data.gates.size();
    gate_buffer_.bind();
    if (gate_bytes <= gate_capacity_bytes_) {
        gate_buffer_.update_data(scan_data.gates.data(), gate_bytes);
    } else {
        gate_buffer_.set_data(scan_data.gates.data(), gate_bytes, Buffer::Usage::DynamicDraw);
        gate_capacity_bytes_ = gate_bytes;
    }

    const size_t meta_bytes = sizeof(float) * scan_data.meta_packed.size();
    meta_buffer_.bind();
    if (meta_bytes <= meta_capacity_bytes_) {
        meta_buffer_.update_data(scan_data.meta_packed.data(), meta_bytes);
    } else {
        meta_buffer_.set_data(scan_data.meta_packed.data(), meta_bytes, Buffer::Usage::DynamicDraw);
        meta_capacity_bytes_ = meta_bytes;
    }
    meta_texture_.set_buffer(GL_RGBA32F, meta_buffer_.id());
}

void MomentRenderer::draw(const ViewProjection &view, const Texture &lut,
                          const ProductRenderConfig &config) {
    if (gate_count_ == 0 || radial_count_ == 0) return;

    shader_.use();
    shader_.set_vec2("u_view_scale", view.scale_x, view.scale_y);
    shader_.set_vec2("u_view_offset", view.offset_x, view.offset_y);
    shader_.set_float("u_min_value", config.min_value);
    shader_.set_float("u_max_value", config.max_value);
    shader_.set_float("u_discard_below", config.discard_below);
    shader_.set_vec2("u_storm_motion", config.storm_u, config.storm_v);
    meta_texture_.bind_unit(0);
    lut.bind_unit(1);
    vao_.bind();
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(gate_count_));
}

bool OverlayRenderer::initialize(float max_range) {
    vao_.create();
    vao_.bind();
    vbo_.bind();
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    if (!shader_.load_files("shaders/overlay.vert", "shaders/overlay.frag")) {
        return false;
    }
    update_range(max_range);
    return true;
}

void OverlayRenderer::update_range(float max_range) {
    if (std::fabs(max_range - range_key_) < 1.0f) return;
    geometry_ = build_geometry(max_range);
    upload(geometry_);
    range_key_ = max_range;
}

void OverlayRenderer::draw(const ViewProjection &view) {
    shader_.use();
    shader_.set_vec2("u_view_scale", view.scale_x, view.scale_y);
    shader_.set_vec2("u_view_offset", view.offset_x, view.offset_y);
    vao_.bind();

    shader_.set_vec4("u_color", 0.55f, 0.60f, 0.68f, 0.55f);
    for (size_t i = 0; i < geometry_.ring_offsets.size(); ++i) {
        glDrawArrays(GL_LINE_LOOP, geometry_.ring_offsets[i], geometry_.ring_counts[i]);
    }

    if (geometry_.marker_count > 0) {
        shader_.set_vec4("u_color", 1.0f, 1.0f, 1.0f, 0.95f);
        glDrawArrays(GL_LINES, geometry_.marker_offset, geometry_.marker_count);
    }
}

OverlayRenderer::OverlayGeometry OverlayRenderer::build_geometry(float max_range) {
    OverlayGeometry o;
    constexpr int kSegments = 192;
    constexpr float kTwoPi = 6.28318530718f;
    const std::array<float, 5> ring_km = {25.0f, 50.0f, 100.0f, 150.0f, 200.0f};

    for (float rk : ring_km) {
        const float r_m = rk * 1000.0f;
        if (r_m > max_range * 1.05f) continue;
        const int start = static_cast<int>(o.verts.size() / 2);
        for (int s = 0; s < kSegments; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(kSegments);
            const float a = t * kTwoPi;
            o.verts.push_back(std::cos(a) * r_m);
            o.verts.push_back(std::sin(a) * r_m);
        }
        o.ring_offsets.push_back(start);
        o.ring_counts.push_back(kSegments);
    }

    constexpr float kArm = 5000.0f;
    constexpr float kGap = 800.0f;
    o.marker_offset = static_cast<int>(o.verts.size() / 2);
    const float marker_pts[] = {
         kGap, 0.0f,  kArm, 0.0f,
        -kGap, 0.0f, -kArm, 0.0f,
         0.0f, kGap,  0.0f, kArm,
         0.0f,-kGap,  0.0f,-kArm,
    };
    for (float v : marker_pts) o.verts.push_back(v);
    o.marker_count = static_cast<int>(sizeof(marker_pts) / sizeof(float) / 2);

    return o;
}

void OverlayRenderer::upload(const OverlayGeometry &geometry) {
    vao_.bind();
    vbo_.bind();
    vbo_.set_data(geometry.verts.data(), sizeof(float) * geometry.verts.size(),
                  Buffer::Usage::StaticDraw);
}

bool LegendRenderer::initialize() {
    vao_.create();
    vao_.bind();
    vbo_.bind();
    vbo_.set_data(kQuadVertices, sizeof(kQuadVertices), Buffer::Usage::StaticDraw);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    if (!shader_.load_files("shaders/legend.vert", "shaders/legend.frag")) {
        return false;
    }
    shader_.use();
    shader_.set_int("u_value_lut", 1);
    return true;
}

void LegendRenderer::draw(int framebuffer_width, int framebuffer_height,
                          const Texture &lut, const ProductRenderConfig &config) {
    if (framebuffer_width <= 0 || framebuffer_height <= 0) return;

    const float bar_w = 18.0f;
    const float bar_margin = 16.0f;
    const float bar_h = std::max(80.0f, static_cast<float>(framebuffer_height) * 0.6f);
    const float bar_x = static_cast<float>(framebuffer_width) - bar_margin - bar_w;
    const float bar_y = (static_cast<float>(framebuffer_height) - bar_h) * 0.5f;

    shader_.use();
    shader_.set_vec4("u_pixel_rect", bar_x, bar_y, bar_w, bar_h);
    shader_.set_vec2("u_screen_size", static_cast<float>(framebuffer_width),
                     static_cast<float>(framebuffer_height));
    shader_.set_vec2("u_pixel_size", bar_w, bar_h);
    shader_.set_float("u_min_value", config.min_value);
    shader_.set_float("u_max_value", config.max_value);
    shader_.set_float("u_tick_period", config.tick_period);
    lut.bind_unit(1);
    vao_.bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
