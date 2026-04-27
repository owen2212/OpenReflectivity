#include <memory>
#include <string>
#include <stdexcept>
#include <utility>

#include "rsl_wrapper.hpp"
// C API
extern "C" {
    #include "rsl.h"
}

namespace rsl {

struct RadarHandle {
    Radar *r = nullptr;
};

static std::vector<Scan> get_scans_from_vol(const Volume *vol);
static std::vector<Radial> get_radials_from_sweep(const Sweep *sweep, const Volume *vol);

void RadarData::RadarDeleter::operator()(RadarHandle *r) const noexcept {
    if (r) {
        RSL_free_radar(r->r);
        delete r;
    }
}

RadarData::RadarData(const std::string& file_path, const std::string& radar_site)
    : radar_ptr(new RadarHandle{RSL_wsr88d_to_radar(const_cast<char*>(file_path.c_str()), const_cast<char*>(radar_site.c_str()))})
{
    if (!radar_ptr || !radar_ptr->r) {
        throw std::runtime_error("Could not load level 2 archive file: " + file_path);
    }
}

Product RadarData::get_product(ProductType product_type) {
    Product p;

    Volume *vol = nullptr;
    switch (product_type) {
        case ProductType::REFLECTIVITY:
            vol = radar_ptr->r->v[DZ_INDEX];
            break;
        case ProductType::VELOCITY:
            vol = radar_ptr->r->v[VL_INDEX];
            break;
        case ProductType::SPECTRAL_WIDTH:
            vol = radar_ptr->r->v[SW_INDEX];
            break;
    }

    if (!vol) {
        throw std::runtime_error("Requested product data is missing");
    }

    p.scans = get_scans_from_vol(vol);
    return p;
}

static std::vector<Scan> get_scans_from_vol(const Volume *vol) {
    std::vector<Scan> scans;
    scans.reserve(static_cast<size_t>(vol->h.nsweeps));

    for (int i = 0; i < vol->h.nsweeps; ++i) {
        Sweep *sweep = vol->sweep[i];
        if (!sweep) continue;

        Scan scan;
        scan.radials = get_radials_from_sweep(sweep, vol);
        scan.elevation = sweep->h.elev;
        scans.push_back(std::move(scan));
    }

    return scans;
}

// prefer ray->h.f, then sweep->h.f, then vol->h.f
static inline float (*pick_f(const Ray* ray, const Sweep* sweep, const Volume* vol))(Range) {
    if (ray && ray->h.f) return ray->h.f;
    if (sweep && sweep->h.f) return sweep->h.f;
    if (vol && vol->h.f) return vol->h.f;
    return nullptr;
}

static std::vector<Radial> get_radials_from_sweep(const Sweep *sweep, const Volume *vol) {
    std::vector<Radial> radials;
    radials.reserve(static_cast<size_t>(sweep->h.nrays));

    for (int i = 0; i < sweep->h.nrays; ++i) {
        Ray *ray = sweep->ray[i];
        if (!ray) continue;

        auto f = pick_f(ray, sweep, vol);
        if (!f) continue;

        Radial radial;
        radial.gates.reserve(static_cast<size_t>(ray->h.nbins));
        for (int j = 0; j < ray->h.nbins; ++j) {
            float gate = f(ray->range[j]);
            if (gate == BADVAL || gate == RFVAL || gate == APFLAG || gate == NOECHO) {
                gate = SENTINEL;
            }
            radial.gates.push_back(gate);
        }

        radial.azimuth = ray->h.azimuth;
        radial.gate_size = ray->h.gate_size;
        radial.range_bin1 = ray->h.range_bin1;
        radials.push_back(std::move(radial));
    }

    return radials;
}

} // namespace rsl
