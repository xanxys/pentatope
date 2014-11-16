#include "material.h"

namespace pentatope {

Material::~Material() {
}


// refl: [0, 1] value.
UniformLambertMaterial::UniformLambertMaterial(const Spectrum& refl) :
		refl(refl) {
    if(refl.minCoeff() < 0 || refl.maxCoeff() > 1) {
        throw physics_error("Don't create non energy conserving lambertian BSDF.");
    }
}

std::unique_ptr<BSDF> UniformLambertMaterial::getBSDF(
		const MicroGeometry& geom) {
    return std::unique_ptr<BSDF>(new LambertBRDF(geom, refl));
}


UniformEmissionMaterial::UniformEmissionMaterial(
		const Spectrum& emission_radiance) :
		e_radiance(emission_radiance) {
}

std::unique_ptr<BSDF> UniformEmissionMaterial::getBSDF(
		const MicroGeometry& geom) {
    return std::unique_ptr<BSDF>(new EmissionBRDF(geom, e_radiance));
}


GlassMaterial::GlassMaterial(float refractive_index) :
	refractive_index(refractive_index) {
}

std::unique_ptr<BSDF> GlassMaterial::getBSDF(const MicroGeometry& geom) {
	return std::unique_ptr<BSDF>(new RefractiveBTDF(geom, refractive_index));
}

}  // namespace
