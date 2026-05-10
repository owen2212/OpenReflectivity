#include "map_renderer.hpp"

#include <glad/glad.h>

bool MapRenderer::initialize() {
    if (!shader_.load_files("shaders/overlay.vert", "shaders/overlay.frag")) {
        return false;
    }
    vao_.create();
    vao_.bind();
    vbo_.bind();
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    vao_.unbind();
    return true;
}

void MapRenderer::set_layers(std::vector<Layer> &&layers) {
    spans_.clear();

    // concat all layers into one VBO, recording per-layer draw spans with
    // first indices rebased onto the combined buffer
    std::vector<float> all_verts;
    size_t total = 0;
    for (const Layer &layer : layers) total += layer.lines.verts.size();
    all_verts.reserve(total);

    for (Layer &layer : layers) {
        LayerSpan span;
        span.r = layer.r;
        span.g = layer.g;
        span.b = layer.b;
        span.a = layer.a;
        span.min_zoom = layer.min_zoom;
        const int base = static_cast<int>(all_verts.size() / 2);
        span.first.reserve(layer.lines.first.size());
        for (size_t i = 0; i < layer.lines.first.size(); ++i) {
            span.first.push_back(layer.lines.first[i] + base);
        }
        span.count = layer.lines.count;
        all_verts.insert(all_verts.end(), layer.lines.verts.begin(), layer.lines.verts.end());
        spans_.push_back(std::move(span));
    }

    vbo_.bind();
    vbo_.set_data(all_verts.empty() ? nullptr : all_verts.data(),
                  all_verts.size() * sizeof(float),
                  Buffer::Usage::StaticDraw);
}

void MapRenderer::draw(const ViewProjection &view, float zoom) {
    if (spans_.empty()) return;

    shader_.use();
    shader_.set_vec2("u_view_scale", view.scale_x, view.scale_y);
    shader_.set_vec2("u_view_offset", view.offset_x, view.offset_y);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    vao_.bind();
    for (const LayerSpan &span : spans_) {
        if (span.first.empty() || zoom < span.min_zoom) continue;
        shader_.set_vec4("u_color", span.r, span.g, span.b, span.a);
        glMultiDrawArrays(GL_LINE_STRIP, span.first.data(), span.count.data(),
                          static_cast<GLsizei>(span.first.size()));
    }
    vao_.unbind();
}
