#ifndef RSL_WRAPPER_HPP
#define RSL_WRAPPER_HPP

#include <memory>
#include <string>
#include <vector>

/**
 * Namespace for all RSL wrapper utilities 
 */
namespace rsl{

// Forward declaration for opaque handle (defined in .cpp)
struct RadarHandle;

enum PRODUCT_TYPE{
    REFLECTIVITY,
    VELOCITY,
    SPECTRAL_WIDTH
};

// Roughly equivalent to RSL Ray
typedef struct {
    std::vector<float> gates;
    float azimuth;
    float range_bin1;
    float gate_size;
    // TODO: add metadata (azimuth angle, etc.)
} Radial;

// Roughly equivalent to RSL Sweep
typedef struct {
    std::vector<Radial> radials;
    float elevation;
    // TODO: add metadata (elevation, etc.)
} Scan;

// Roughly equivalent to RSL Volume
typedef struct {
    std::vector<Scan> scans;
} Product;

// RAII wrapper around Radar*
class RadarData{
    public:
        // RAII - no default constructor
        RadarData() = delete;
        RadarData(const std::string& file_path, const std::string& radar_site);
        /**
         * @fn get_product
         * Gets the entirety of a radar product (reflectivity, velocity, sw)
         * @param product_type  PRODUCT_TYPE enum indicatinng product selection
         * @returns Product object with the radar data
         */
        Product get_product(PRODUCT_TYPE product_type);

    private:
        // Deleter functor for Radar
        struct RadarDeleter{
            void operator()(RadarHandle *r) const noexcept;
        };
        std::unique_ptr<RadarHandle, RadarDeleter> radar_ptr;
};

};

#endif
