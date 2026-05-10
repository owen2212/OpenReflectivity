#ifndef MAP_RENDERER_HPP
#define MAP_RENDERER_HPP

#include <vector>

#include "app/map_data.hpp"
#include "app/renderers.hpp"
#include "gl/buffer.hpp"
#include "gl/shader.hpp"
#include "gl/vertex_array.hpp"

// draws state/county lines in world space with the moment renderer's
// pan/zoom. Call set_layers again after a site change re-projection.
class MapRenderer {
    public:
        struct Layer {
            ProjectedMapLines lines;
            float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
            float min_zoom = 0.0f;  // layer hidden below this zoom level
        };

        bool initialize();
        void set_layers(std::vector<Layer> &&layers);
        void draw(const ViewProjection &view, float zoom);

    private:
        VertexArray vao_;
        Buffer vbo_{Buffer::Target::Array};
        Shader shader_;
        struct LayerSpan {
            std::vector<int> first;
            std::vector<int> count;
            float r, g, b, a;
            float min_zoom;
        };
        std::vector<LayerSpan> spans_;
};

#endif
