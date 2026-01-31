#include <memory>
#include <string>
#include <stdexcept>

#include "rsl_wrapper.hpp"
// C API
extern "C" {
    #include "rsl.h"
}

namespace rsl {

/**
 * Implementation
 * Custom deleter for Radar to enforce RAII with unique_ptr
 */
void RadarData::RadarDeleter::operator()(Radar *r) const noexcept{
    if(r) RSL_free_radar(r);
}

RadarData::RadarData(const std::string& file_path, const std::string& radar_site)
    : radar_ptr(RSL_wsr88d_to_radar(const_cast<char*>(file_path.c_str()), const_cast<char*>(radar_site.c_str())))
{
    if(!radar_ptr){
        throw std::runtime_error("Could not load level 2 archive file: " + file_path);
    }

}

Product RadarData::get_product(PRODUCT_TYPE product_type) {
    Product p;

    Volume *vol = nullptr;
    switch(product_type){
        case PRODUCT_TYPE::REFLECTIVITY:
            vol = radar_ptr->v[DZ_INDEX];
            break;
        case PRODUCT_TYPE::VELOCITY:
            vol = radar_ptr->v[VL_INDEX];
            break;
        case PRODUCT_TYPE::SPECTRAL_WIDTH:
            vol = radar_ptr->v[SW_INDEX];
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

std::vector<Scan> get_scans_from_vol(Volume *vol){
    std::vector<Scan> scans;

    for(int i=0; i<vol->h.nsweeps; ++i){ 
        Scan scan;
        Sweep *sweep = vol->sweep[i];
        std::vector<Radial> radials = get_radials_from_sweep(sweep);
        scan.radials = radials; 
        scan.elevation = sweep->h.elev;
        scans.push_back(scan);
    }

    return scans;
}

std::vector<Radial> get_radials_from_sweep(Sweep *sweep){
    std::vector<Radial> radials;

    // Go thru each radial
    for(int i=0; i<sweep->h.nrays; ++i){
        Radial radial;
        std::vector<float> gates;
        Ray *ray = sweep->ray[i];
        // Go thru each gate and convert to float 
        for(int j=0; j<ray->h.nbins; ++j){
            Range pre_gate = ray->range[j]; // Range is unsigned short
            float gate = sweep->h.f(pre_gate);
            gates.push_back(gate);
        }

        radial.gates = gates;
        radial.azimuth = ray->h.azimuth;
        radials.push_back(radial);
    }

    return radials;
}

};
