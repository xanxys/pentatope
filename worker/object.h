#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <boost/optional.hpp>
#include <Eigen/Dense>
#include <glog/logging.h>

#include <geometry.h>
#include <light.h>
#include <material.h>
#include <sampling.h>
#include <space.h>

namespace pentatope {

using Object = std::pair<
    std::unique_ptr<Geometry>,
    std::unique_ptr<Material>>;

}  // namespace
