#ifndef RSL_WRAPPER_HPP
#define RSL_WRAPPER_HPP

#include <memory>
#include <string>
#include <vector>

// Forward declaration
struct Radar;
struct Volume;
struct Sweep;

/**
 * Namespace for all RSL wrapper utilities 
 */
namespace rsl{

enum PRODUCT_TYPE{
    REFLECTIVITY,
    VELOCITY,
    SPECTRAL_WIDTH
};

// Roughly equivalent to RSL Ray
typedef struct {
    std::vector<float> gates;
    float azimuth;
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

// Generation functions for above typedefs
std::vector<Scan> get_scans_from_vol(Volume *vol);
std::vector<Radial> get_radials_from_sweep(Sweep *sweep);

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
            void operator()(Radar *r) const noexcept;
        };
        std::unique_ptr<Radar, RadarDeleter> radar_ptr;
};

};

#endif