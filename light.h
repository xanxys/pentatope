// Defines light, color and radiometry in 4-d space.
//
// BSDF is a description of light at a single point,
// why Material (not contained here) are distribution of BSDF
// over geometry and/or space.
#pragma once

#include <Eigen/Dense>

#include <geometry.h>
#include <space.h>

namespace pentatope {

// Currently RGB.
// TODO: use properly defined values.
using Spectrum = Eigen::Vector3f;

Spectrum fromRgb(float r, float g, float b);


// BSDF at particular point + emission.
// Contains MicroGeometry.
// Note that geom can be different from raw MicroGeometry
// obtained from Geometry, for example when using normal maps.
// Material should handle such MicroGeometry transformation.
class BSDF {
public:
    BSDF(const MicroGeometry& geom);
    virtual ~BSDF();

    virtual Spectrum bsdf(
        const Eigen::Vector4f& dir_in, const Eigen::Vector4f& dir_out) const;

    virtual Spectrum emission(const Eigen::Vector4f& dir_out) const;
protected:
    MicroGeometry geom;
};


class LambertBRDF : public BSDF {
public:
    // refl: [0, 1] value.
    LambertBRDF(const MicroGeometry& geom, const Spectrum& refl);

    Spectrum bsdf(const Eigen::Vector4f& dir_in, const Eigen::Vector4f& dir_out) const override;
private:
    Spectrum refl;
    Spectrum refl_normalized;
};


// uniform emission with no reflection nor transparency.
class EmissionBRDF : public BSDF {
public:
    EmissionBRDF(const MicroGeometry& geom, const Spectrum& e_radiance);

    Spectrum emission(const Eigen::Vector4f& dir_out) const override;
private:
    Spectrum e_radiance;
};


}  // namespace
