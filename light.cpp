#include "light.h"

namespace pentatope {

Spectrum fromRgb(float r, float g, float b) {
    return Spectrum(r, g, b);
}

BSDF::BSDF(const MicroGeometry& geom) : geom(geom) {
}

BSDF::~BSDF() {
}

Spectrum BSDF::bsdf(
        const Eigen::Vector4f& dir_in, const Eigen::Vector4f& dir_out) const {
    return Spectrum::Zero();
}
Spectrum BSDF::emission(const Eigen::Vector4f& dir_out) const {
    return Spectrum::Zero();
}


// refl: [0, 1] value.
LambertBRDF::LambertBRDF(const MicroGeometry& geom, const Spectrum& refl) : BSDF(geom), refl(refl) {
    refl_normalized = refl * (3 / (4 * pi));
}

Spectrum LambertBRDF::bsdf(const Eigen::Vector4f& dir_in, const Eigen::Vector4f& dir_out) const {
    return refl_normalized;
}


EmissionBRDF::EmissionBRDF(const MicroGeometry& geom, const Spectrum& e_radiance) :
        BSDF(geom), e_radiance(e_radiance) {
}

Spectrum EmissionBRDF::emission(const Eigen::Vector4f& dir_out) const {
    return e_radiance;
}


}  // namespace
