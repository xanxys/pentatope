#include "scene.h"

#include <random>

#include <boost/range/irange.hpp>
#include <gtest/gtest.h>


std::vector<pentatope::Object> arbitraryObjects() {
    std::mt19937 rg;
    const int n = std::uniform_int_distribution<int>(1, 100)(rg);
    std::vector<pentatope::Object> objects;
    for(const int i : boost::irange(0, n)) {
        objects.emplace_back(
            std::make_unique<pentatope::Sphere>(
                Eigen::Vector4f(
                    std::uniform_real_distribution<float>(-100, 100)(rg),
                    std::uniform_real_distribution<float>(-100, 100)(rg),
                    std::uniform_real_distribution<float>(-100, 100)(rg),
                    std::uniform_real_distribution<float>(-100, 100)(rg)),
                std::uniform_real_distribution<float>(1e-3, 10)(rg)),
            std::make_unique<pentatope::UniformLambertMaterial>(
                pentatope::fromRgb(1, 1, 1)));
    }
    return objects;
}

pentatope::Ray arbitraryRay() {
    std::mt19937 rg;
    Eigen::Vector4f org(
        std::uniform_real_distribution<float>(-100, 100)(rg),
        std::uniform_real_distribution<float>(-100, 100)(rg),
        std::uniform_real_distribution<float>(-100, 100)(rg),
        std::uniform_real_distribution<float>(-100, 100)(rg));
    Eigen::Vector4f dir(
        std::uniform_real_distribution<float>(-1, 1)(rg),
        std::uniform_real_distribution<float>(-1, 1)(rg),
        std::uniform_real_distribution<float>(-1, 1)(rg),
        std::uniform_real_distribution<float>(-1, 1)(rg));
    dir.normalize();
    return pentatope::Ray(org, dir);
}

TEST(BVHAccel, ConstructsProperly) {
    const auto objs = arbitraryObjects();
    auto bvh = std::make_unique<pentatope::BVHAccel>();
    bvh->build(objs);
}

TEST(BVHAccel, BehaveIdenticallyToBruteForce) {
    for(const int i : boost::irange(0, 100)) {
        const auto objs = arbitraryObjects();

        auto truth = std::make_unique<pentatope::BruteForceAccel>();
        truth->build(objs);

        auto bvh = std::make_unique<pentatope::BVHAccel>();
        bvh->build(objs);

        const auto ray = arbitraryRay();
        const auto isect_truth = truth->intersect(ray);
        const auto isect_bvh = bvh->intersect(ray);
        // Existence of intersection.
        EXPECT_EQ(static_cast<bool>(isect_truth.first),
            static_cast<bool>(isect_bvh.first));
        if(isect_truth.first) {
            EXPECT_EQ(isect_truth.second.pos(), isect_bvh.second.pos());
            EXPECT_EQ(isect_truth.second.normal(), isect_bvh.second.normal());
        }
    }
}
