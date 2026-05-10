#include "map_data.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>

namespace {

bool read_exact(std::ifstream &in, void *dst, size_t bytes) {
    in.read(static_cast<char*>(dst), static_cast<std::streamsize>(bytes));
    return in.good();
}

bool check_magic(std::ifstream &in, const char expected[4], const std::string &path) {
    char magic[4] = {};
    uint32_t version = 0;
    if (!read_exact(in, magic, 4) || std::memcmp(magic, expected, 4) != 0) {
        std::fprintf(stderr, "Bad magic in map file %s\n", path.c_str());
        return false;
    }
    if (!read_exact(in, &version, 4) || version != 1) {
        std::fprintf(stderr, "Unsupported map file version in %s\n", path.c_str());
        return false;
    }
    return true;
}

} // namespace

bool load_polyline_file(const std::string &path, MapPolylineLayer &out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "Map layer not found: %s (map overlay disabled)\n", path.c_str());
        return false;
    }
    if (!check_magic(in, "ORLN", path)) return false;

    uint32_t polyline_count = 0;
    uint32_t vertex_count = 0;
    if (!read_exact(in, &polyline_count, 4) || !read_exact(in, &vertex_count, 4)) return false;

    out.counts.resize(polyline_count);
    out.lonlat.resize(static_cast<size_t>(vertex_count) * 2);
    if (polyline_count > 0 &&
        !read_exact(in, out.counts.data(), polyline_count * sizeof(uint32_t))) return false;
    if (vertex_count > 0 &&
        !read_exact(in, out.lonlat.data(), out.lonlat.size() * sizeof(float))) return false;

    size_t sum = 0;
    for (uint32_t c : out.counts) sum += c;
    if (sum != vertex_count) {
        std::fprintf(stderr, "Corrupt map file %s\n", path.c_str());
        out = {};
        return false;
    }
    return true;
}

bool load_places_file(const std::string &path, std::vector<MapPlace> &out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "Places file not found: %s (city labels disabled)\n", path.c_str());
        return false;
    }
    if (!check_magic(in, "ORPT", path)) return false;

    uint32_t count = 0;
    if (!read_exact(in, &count, 4)) return false;
    out.clear();
    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        MapPlace p;
        uint8_t name_len = 0;
        if (!read_exact(in, &p.lon, 4) || !read_exact(in, &p.lat, 4) ||
            !read_exact(in, &p.rank, 1) || !read_exact(in, &name_len, 1)) {
            out.clear();
            return false;
        }
        p.name.resize(name_len);
        if (name_len > 0 && !read_exact(in, p.name.data(), name_len)) {
            out.clear();
            return false;
        }
        out.push_back(std::move(p));
    }
    return true;
}

ProjectedMapLines project_lines(const MapPolylineLayer &layer,
                                const AzimuthalEquidistant &proj,
                                float max_radius_m) {
    ProjectedMapLines out;
    out.verts.reserve(layer.lonlat.size());

    size_t vertex_base = 0;
    for (uint32_t count : layer.counts) {
        float min_x = std::numeric_limits<float>::max();
        float min_y = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float max_y = std::numeric_limits<float>::lowest();

        const size_t start_vert = out.verts.size() / 2;
        for (uint32_t i = 0; i < count; ++i) {
            const size_t src = (vertex_base + i) * 2;
            float x = 0.0f;
            float y = 0.0f;
            proj.forward(layer.lonlat[src + 1], layer.lonlat[src], x, y);
            out.verts.push_back(x);
            out.verts.push_back(y);
            min_x = std::min(min_x, x);
            min_y = std::min(min_y, y);
            max_x = std::max(max_x, x);
            max_y = std::max(max_y, y);
        }
        vertex_base += count;

        // keep the polyline if its bbox touches the site radius
        const bool outside =
            (min_x > max_radius_m) || (max_x < -max_radius_m) ||
            (min_y > max_radius_m) || (max_y < -max_radius_m);
        if (outside || count < 2) {
            out.verts.resize(start_vert * 2);
            continue;
        }
        out.first.push_back(static_cast<int>(start_vert));
        out.count.push_back(static_cast<int>(count));
    }
    return out;
}

std::vector<ProjectedPlace> project_places(const std::vector<MapPlace> &places,
                                           const AzimuthalEquidistant &proj,
                                           float max_radius_m) {
    std::vector<ProjectedPlace> out;
    for (const MapPlace &p : places) {
        float x = 0.0f;
        float y = 0.0f;
        proj.forward(p.lat, p.lon, x, y);
        if (std::abs(x) > max_radius_m || std::abs(y) > max_radius_m) continue;
        out.push_back({x, y, p.rank, &p.name});
    }
    return out;
}
