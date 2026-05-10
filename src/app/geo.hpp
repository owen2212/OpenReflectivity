#ifndef GEO_HPP
#define GEO_HPP

// spherical azimuthal-equidistant projection centered on the radar site.
// Matches the renderer's world space (meters, +x east, +y north). Range
// error is under ~1% at WSR-88D display ranges.
class AzimuthalEquidistant {
    public:
        AzimuthalEquidistant() = default;
        AzimuthalEquidistant(double lat0_deg, double lon0_deg);

        void forward(double lat_deg, double lon_deg, float &x_m, float &y_m) const;
        void inverse(float x_m, float y_m, double &lat_deg, double &lon_deg) const;

    private:
        double lat0_rad_ = 0.0;
        double lon0_rad_ = 0.0;
        double sin_lat0_ = 0.0;
        double cos_lat0_ = 1.0;
};

#endif
