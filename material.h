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

}  // namespace
