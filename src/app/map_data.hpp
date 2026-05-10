#ifndef MAP_DATA_HPP
#define MAP_DATA_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "app/geo.hpp"

// raw layers keep lon/lat so they can be re-projected when the site changes

struct MapPolylineLayer {
    std::vector<uint32_t> counts;   // vertices per polyline
    std::vector<float> lonlat;      // interleaved lon, lat (degrees)
};

struct MapPlace {
    float lon = 0.0f;
    float lat = 0.0f;
    uint8_t rank = 255;             // Natural Earth SCALERANK, 0 = largest
    std::string name;
};

struct ProjectedMapLines {
    std::vector<float> verts;       // interleaved x, y meters
    std::vector<int> first;         // per polyline, for glMultiDrawArrays
    std::vector<int> count;
};

struct ProjectedPlace {
    float x = 0.0f;                 // world meters
    float y = 0.0f;
    uint8_t rank = 255;
    const std::string *name = nullptr;  // points into the raw MapPlace
};

bool load_polyline_file(const std::string &path, MapPolylineLayer &out);
bool load_places_file(const std::string &path, std::vector<MapPlace> &out);

// projects, dropping polylines entirely beyond max_radius_m of the site.
// Coarse per-polyline cull, lines that merely extend past it are kept.
ProjectedMapLines project_lines(const MapPolylineLayer &layer,
                                const AzimuthalEquidistant &proj,
                                float max_radius_m);

std::vector<ProjectedPlace> project_places(const std::vector<MapPlace> &places,
                                           const AzimuthalEquidistant &proj,
                                           float max_radius_m);

#endif
