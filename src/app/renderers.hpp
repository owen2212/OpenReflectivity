#ifndef RENDERERS_HPP
#define RENDERERS_HPP

#include <cstddef>
#include <vector>

#include "app/radar_render_data.hpp"
#include "gl/buffer.hpp"
#include "gl/shader.hpp"
#include "gl/texture.hpp"
#include "gl/vertex_array.hpp"

struct ViewProjection {
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

class ReflectivityRenderer {
    public:
        bool initialize();
        void upload_scan(const ScanGpuData &scan_data);
        void draw(const ViewProjection &view, const Texture &dbz_lut);
        float max_range() const { return max_range_; }

    private:
        VertexArray vao_;
        Buffer gate_buffer_{Buffer::Target::Array};
        Buffer quad_buffer_{Buffer::Target::Array};
        Buffer meta_buffer_{Buffer::Target::Array};
        Texture meta_texture_{Texture::Target::TextureBuffer};
        Shader shader_;
        size_t gate_count_ = 0;
        size_t radial_count_ = 0;
        size_t gate_capacity_bytes_ = 0;
        size_t meta_capacity_bytes_ = 0;
        float max_range_ = 0.0f;
};

class OverlayRenderer {
    public:
        bool initialize(float max_range);
        void update_range(float max_range);
        void draw(const ViewProjection &view);

    private:
        struct OverlayGeometry {
            std::vector<float> verts;
            std::vector<int> ring_offsets;
            std::vector<int> ring_counts;
            int marker_offset = 0;
            int marker_count = 0;
        };

        static OverlayGeometry build_geometry(float max_range);
        void upload(const OverlayGeometry &geometry);

        VertexArray vao_;
        Buffer vbo_{Buffer::Target::Array};
        Shader shader_;
        OverlayGeometry geometry_;
        float range_key_ = -1.0f;
};

class LegendRenderer {
    public:
        bool initialize();
        void draw(int framebuffer_width, int framebuffer_height, const Texture &dbz_lut);

    private:
        VertexArray vao_;
        Buffer vbo_{Buffer::Target::Array};
        Shader shader_;
};

Texture create_dbz_lut_texture();
ViewProjection make_view_projection(float max_range, int framebuffer_width, int framebuffer_height,
                                    float zoom, float offset_x, float offset_y);

#endif
