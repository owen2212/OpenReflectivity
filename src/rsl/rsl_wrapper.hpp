#ifndef RSL_WRAPPER_HPP
#define RSL_WRAPPER_HPP

#include <memory>
#include <string>
#include <vector>

namespace rsl {

// Forward declaration for opaque handle (defined in .cpp)
struct RadarHandle;

constexpr float SENTINEL = -9999.0f;

enum class ProductType {
    REFLECTIVITY,
    VELOCITY,
    SPECTRAL_WIDTH
};

struct Radial {
    std::vector<float> gates;
    float azimuth;
    float range_bin1;
    float gate_size;
};

struct Scan {
    std::vector<Radial> radials;
    float elevation;
};

struct Product {
    std::vector<Scan> scans;
};

// RAII wrapper around Radar*
class RadarData {
    public:
        RadarData() = delete;
        RadarData(const std::string& file_path, const std::string& radar_site);
        Product get_product(ProductType product_type);

    private:
        struct RadarDeleter {
            void operator()(RadarHandle *r) const noexcept;
        };
        std::unique_ptr<RadarHandle, RadarDeleter> radar_ptr;
};

} // namespace rsl

#endif
