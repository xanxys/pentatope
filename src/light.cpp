#include "light.h"

namespace pentatope {

Spectrum fromRgb(float r, float g, float b) {
    return Spectrum(r, g, b);
}

BSDF::BSDF(const MicroGeometry& geom) : geom(geom) {
}

BSDF::~BSDF() {
}

boost::optional<std::pair<Eigen::Vector4f, Spectrum>>
        BSDF::specular(const Eigen::Vector4f& dir_out) const {
    return boost::none;
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


RefractiveBTDF::RefractiveBTDF(
		const MicroGeometry& geom, float refractive_index) :
         BSDF(geom), refractive_index(refractive_index) {
    if(refractive_index <= 0) {
        throw physics_error("Negative refractive index not allowed (yet)");
    }
}

boost::optional<std::pair<Eigen::Vector4f, Spectrum>>
        RefractiveBTDF::specular(const Eigen::Vector4f& dir_out) const {
    const float dout_cos = geom.normal().dot(dir_out);
    assert(-1 <= dout_cos && dout_cos <= 1);
    // almost parallel to normal.
    if(std::abs(dout_cos) >= 1 - 1e-3) {
        return boost::optional<std::pair<Eigen::Vector4f, Spectrum>>(
            std::make_pair(-dir_out, Spectrum::Ones()));
    }

    // Non-parallel: use Snell's law.
    const float dout_sin = std::sqrt(1 - std::pow(dout_cos, 2));
    assert(0 <= dout_sin && dout_sin <= 1);

    // entering vs leaving.
    const float rri = (dout_cos > 0) ? refractive_index : 1 / refractive_index;
    Eigen::Vector4f dout_proj = dout_cos * geom.normal();
    Eigen::Vector4f dout_perp = dir_out - dout_proj;
    dout_proj.normalize();
    dout_perp.normalize();

    const float din_sin = dout_sin / rri;
    // handle total internal reflection
    if(din_sin > 1) {
        return boost::optional<std::pair<Eigen::Vector4f, Spectrum>>(
            std::make_pair(
                dout_proj * dout_cos - dout_perp * dout_sin,
                Spectrum::Ones()));
    }

    // normal refraction
    const float din_cos = std::sqrt(1 - std::pow(din_sin, 2));
    assert(0 <= din_sin && din_sin <= 1);
    assert(0 <= din_cos && din_cos <= 1);
    return boost::optional<std::pair<Eigen::Vector4f, Spectrum>>(
        std::make_pair(
            -dout_proj * din_cos - dout_perp * din_sin,
            Spectrum::Ones()));
}


PointLight::PointLight(const Eigen::Vector4f& pos, const Spectrum& power) :
        pos(pos), intensity(power / (2 * pi * pi)) {
}

float PointLight::power() const {
    return intensity.norm() * (2 * pi * pi);
}

std::pair<Eigen::Vector4f, Spectrum> PointLight::getIntensity(
        const Eigen::Vector4f& pos_surf) const {
    return std::make_pair(pos, intensity);
}

}  // namespace
