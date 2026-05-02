#include <cmath>
#include <cstring>
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
            vol = radar_ptr->r->v[VR_INDEX];
            break;
        case ProductType::SPECTRAL_WIDTH:
            vol = radar_ptr->r->v[SW_INDEX];
            break;
        case ProductType::COUNT:
            break;
    }

    if (!vol) {
        throw std::runtime_error("Requested product data is missing");
    }

    p.scans = get_scans_from_vol(vol);
    return p;
}

// RSL's signed degree/minute/second triple to decimal degrees. The sign
// lives on the first nonzero component only (e.g. -97d 16m 41s).
static double dms_to_decimal(int d, int m, int s) {
    const double sign = (d < 0 || m < 0 || s < 0) ? -1.0 : 1.0;
    return sign * (std::abs(d) + std::abs(m) / 60.0 + std::abs(s) / 3600.0);
}

SiteInfo RadarData::site_info() const {
    const Radar_header &h = radar_ptr->r->h;
    SiteInfo info;
    info.lat = dms_to_decimal(h.latd, h.latm, h.lats);
    info.lon = dms_to_decimal(h.lond, h.lonm, h.lons);
    info.height_m = static_cast<double>(h.height);
    info.site_id.assign(h.name, strnlen(h.name, sizeof(h.name)));
    info.vcp = h.vcp;
    return info;
}

static std::vector<Scan> get_scans_from_vol(const Volume *vol) {
    std::vector<Scan> scans;
    scans.reserve(static_cast<size_t>(vol->h.nsweeps));

    // push an empty Scan for NULL slots so scan_idx means the same physical
    // elevation across all moments (split cuts produce NULL sweeps at
    // different positions per volume)
    for (int i = 0; i < vol->h.nsweeps; ++i) {
        Sweep *sweep = vol->sweep[i];
        if (!sweep) {
            scans.emplace_back();
            continue;
        }

        Scan scan;
        scan.radials = get_radials_from_sweep(sweep, vol);
        scan.elevation = sweep->h.elev;
        for (int r = 0; r < sweep->h.nrays; ++r) {
            if (sweep->ray[r]) {
                const Ray_header &rh = sweep->ray[r]->h;
                scan.nyquist_vel = rh.nyq_vel;
                scan.start_time = ScanTime{rh.year, rh.month, rh.day,
                                           rh.hour, rh.minute, rh.sec};
                break;
            }
        }
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
