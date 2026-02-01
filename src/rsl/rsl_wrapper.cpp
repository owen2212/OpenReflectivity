#include <memory>
#include <string>
#include <stdexcept>
#include <cmath>

#include "rsl_wrapper.hpp"
// C API
extern "C" {
    #include "rsl.h"
}

namespace rsl {

struct RadarHandle{
    Radar *r = nullptr;
};

static std::vector<Scan> get_scans_from_vol(const Volume *vol);
static std::vector<Radial> get_radials_from_sweep(const Sweep *sweep, const Volume *vol);

/**
 * Implementation
 * Custom deleter for Radar to enforce RAII with unique_ptr
 */
void RadarData::RadarDeleter::operator()(RadarHandle *r) const noexcept{
    if(r){
        RSL_free_radar(r->r);
        delete r;
    }
}

RadarData::RadarData(const std::string& file_path, const std::string& radar_site)
    : radar_ptr(new RadarHandle{RSL_wsr88d_to_radar(const_cast<char*>(file_path.c_str()), const_cast<char*>(radar_site.c_str()))})
{
    if(!radar_ptr || !radar_ptr->r){
        throw std::runtime_error("Could not load level 2 archive file: " + file_path);
    }

}

/**
 * Implementation
 * Creates Product -> Scans -> Radials
 */
Product RadarData::get_product(PRODUCT_TYPE product_type) {
    Product p;

    Volume *vol = nullptr;
    switch(product_type){
        case REFLECTIVITY:
            vol = radar_ptr->r->v[DZ_INDEX];
            break;
        case VELOCITY:
            vol = radar_ptr->r->v[VL_INDEX];
            break;
        case SPECTRAL_WIDTH:
            vol = radar_ptr->r->v[SW_INDEX];
            break;
        default:
            throw std::runtime_error("Product type not supported");
    }

    if (!vol) {
        throw std::runtime_error("Requested product data is missing");
    }

    p.scans = get_scans_from_vol(vol);
    return p;
}

static std::vector<Scan> get_scans_from_vol(const Volume *vol){
    std::vector<Scan> scans;

    for(int i=0; i<vol->h.nsweeps; ++i){ 
        Scan scan;
        Sweep *sweep = vol->sweep[i];
        if(!sweep) continue;
        std::vector<Radial> radials = get_radials_from_sweep(sweep, vol);
        scan.radials = radials; 
        scan.elevation = sweep->h.elev;
        scans.push_back(scan);
    }

    return scans;
}

// Prefer ray->h.f, then sweep->h.f, then vol->h.f.
static inline float (*pick_f(const Ray* ray, const Sweep* sweep, const Volume* vol))(Range) {
    if (ray && ray->h.f) return ray->h.f;
    if (sweep && sweep->h.f) return sweep->h.f;
    if (vol && vol->h.f) return vol->h.f;
    return nullptr;
}

static std::vector<Radial> get_radials_from_sweep(const Sweep *sweep, const Volume *vol){
    std::vector<Radial> radials;

    // Go thru each radial
    for(int i=0; i<sweep->h.nrays; ++i){
        Radial radial;
        std::vector<float> gates;
        Ray *ray = sweep->ray[i];
        if(!ray) continue;
        auto f = pick_f(ray, sweep, vol);
        if(!f) continue;
        // Go thru each gate and convert to float 
        for(int j=0; j<ray->h.nbins; ++j){
            float gate = sweep->h.f(ray->range[j]);
            if(gate == BADVAL || gate == RFVAL || gate == APFLAG || gate == NOECHO)
                gate = SENTINEL;
            gates.push_back(gate);
        }

        radial.gates = gates;
        radial.azimuth = ray->h.azimuth;
        radial.gate_size = ray->h.gate_size;
        radial.range_bin1 = ray->h.range_bin1;
        radials.push_back(radial);
    }

    return radials;
}

};
