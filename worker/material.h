// Materials are distribution of BSDFs.
// While BSDF can be thought of lambda closures of
// partially evaluated Material, Material
// should have more high-level description.
// For example, transparent BSDF + diffuse BSDF + reflection BSDF
// should be GlassMaterial, instead of generic ComposedMaterial
// of each UniformMaterial corresponding to BSDFs.
//
// Another, more pragmatic reason is that pentatope
// should aim for easy eyecandy while maintaining
// physical consistency, instead of incorporating measured values
// or artist-friendliness of typical 3-d renderers.
#pragma once

#include <memory>

#include <Eigen/Dense>

#include <geometry.h>
#include <light.h>
#include <space.h>

namespace pentatope {

// Material is distribution of BSDF over geometry.
class Material {
public:
    virtual ~Material();
    virtual std::unique_ptr<BSDF>
        getBSDF(const MicroGeometry& geom) = 0;
};


class UniformLambertMaterial : public Material {
public:
    // refl: [0, 1] value.
    UniformLambertMaterial(const Spectrum& refl);
    std::unique_ptr<BSDF> getBSDF(const MicroGeometry& geom) override;
private:
    const Spectrum refl;
};


class UniformEmissionMaterial : public Material {
public:
    UniformEmissionMaterial(const Spectrum& emission_radiance);
    std::unique_ptr<BSDF> getBSDF(const MicroGeometry& geom) override;
private:
    Spectrum e_radiance;
};


// I'm not sure if Snell's law should be encoded in GlassMaterial,
// since refrative index is inherent property of light, not
// some random materials'.
//
// TODO: make refractive index first class citizen
// (space.h or light.h or somewhere like that) if such need arises.
class GlassMaterial : public Material {
public:
    GlassMaterial(float refractive_index);
    std::unique_ptr<BSDF> getBSDF(const MicroGeometry& geom) override;
private:
    float refractive_index;
};


}  // namespace
