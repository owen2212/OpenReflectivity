#include "geo.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kEarthRadiusM = 6371008.8;  // mean Earth radius
constexpr double kDeg2Rad = 0.017453292519943295;
constexpr double kRad2Deg = 57.29577951308232;
}

AzimuthalEquidistant::AzimuthalEquidistant(double lat0_deg, double lon0_deg)
    : lat0_rad_(lat0_deg * kDeg2Rad),
      lon0_rad_(lon0_deg * kDeg2Rad),
      sin_lat0_(std::sin(lat0_deg * kDeg2Rad)),
      cos_lat0_(std::cos(lat0_deg * kDeg2Rad)) {}

void AzimuthalEquidistant::forward(double lat_deg, double lon_deg,
                                   float &x_m, float &y_m) const {
    const double lat = lat_deg * kDeg2Rad;
    const double dlon = lon_deg * kDeg2Rad - lon0_rad_;
    const double sin_lat = std::sin(lat);
    const double cos_lat = std::cos(lat);
    const double cos_dlon = std::cos(dlon);

    const double cos_c = std::clamp(
        sin_lat0_ * sin_lat + cos_lat0_ * cos_lat * cos_dlon, -1.0, 1.0);
    const double c = std::acos(cos_c);
    const double sin_c = std::sin(c);
    const double k = (sin_c < 1e-12) ? 1.0 : c / sin_c;

    x_m = static_cast<float>(kEarthRadiusM * k * cos_lat * std::sin(dlon));
    y_m = static_cast<float>(kEarthRadiusM * k *
                             (cos_lat0_ * sin_lat - sin_lat0_ * cos_lat * cos_dlon));
}

void AzimuthalEquidistant::inverse(float x_m, float y_m,
                                   double &lat_deg, double &lon_deg) const {
    const double x = x_m;
    const double y = y_m;
    const double rho = std::hypot(x, y);
    if (rho < 1e-9) {
        lat_deg = lat0_rad_ * kRad2Deg;
        lon_deg = lon0_rad_ * kRad2Deg;
        return;
    }
    const double c = rho / kEarthRadiusM;
    const double sin_c = std::sin(c);
    const double cos_c = std::cos(c);

    const double lat = std::asin(std::clamp(
        cos_c * sin_lat0_ + (y * sin_c * cos_lat0_) / rho, -1.0, 1.0));
    const double lon = lon0_rad_ + std::atan2(
        x * sin_c, rho * cos_c * cos_lat0_ - y * sin_c * sin_lat0_);

    lat_deg = lat * kRad2Deg;
    lon_deg = lon * kRad2Deg;
}
