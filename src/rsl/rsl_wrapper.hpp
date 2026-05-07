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
    SPECTRAL_WIDTH,
    ZDR,        // differential reflectivity (dB)
    CC,         // correlation coefficient RhoHV (unitless)
    PHI_DP,     // differential phase (deg)
    COUNT
};

struct SiteInfo {
    double lat = 0.0;       // decimal degrees, WGS84; positive north
    double lon = 0.0;       // decimal degrees; positive east
    double height_m = 0.0;
    std::string site_id;
    int vcp = 0;
};

struct ScanTime {
    int year = 0;           // valid iff year > 0
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    float sec = 0.0f;
};

struct Radial {
    std::vector<float> gates;
    float azimuth;
    float range_bin1;
    float gate_size;
};

struct Scan {
    std::vector<Radial> radials;
    float elevation = 0.0f;
    float nyquist_vel = 0.0f;
    ScanTime start_time;
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
        SiteInfo site_info() const;

    private:
        struct RadarDeleter {
            void operator()(RadarHandle *r) const noexcept;
        };
        std::unique_ptr<RadarHandle, RadarDeleter> radar_ptr;
};

} // namespace rsl

#endif
