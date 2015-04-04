#include "scene.h"

#include <chrono>
#include <random>

#include <boost/range/irange.hpp>
#include <gtest/gtest.h>

#include <arbitrary_test.h>


TEST(BVHAccel, ConstructsProperly) {
    std::mt19937 rg;
    const auto objs = arbitraryObjects(rg);
    auto bvh = std::make_unique<pentatope::BVHAccel>();
    bvh->build(objs);
}

TEST(BVHAccel, BehaveIdenticallyToBruteForce) {
    std::mt19937 rg;
    for(const int i : boost::irange(0, 100)) {
        const auto objs = arbitraryObjects(rg);

        auto truth = std::make_unique<pentatope::BruteForceAccel>();
        truth->build(objs);

        auto bvh = std::make_unique<pentatope::BVHAccel>();
        bvh->build(objs);

        const auto ray = arbitraryRay(rg);
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

TEST(BVHAccel, OperatesAtLogN) {
    std::mt19937 rg;
    
    std::vector<pentatope::Ray> rays;
    for(const int i : boost::irange(0, 1000 * 10)) {
        rays.push_back(arbitraryRay(rg));
    }

    auto measure_seconds = [&rays, &rg](int n_objects) {
        const auto objs = arbitraryObjects(rg, n_objects);
        auto bvh = std::make_unique<pentatope::BVHAccel>();
        bvh->build(objs);

        const auto t0 = std::chrono::high_resolution_clock::now();
        for(const auto& ray : rays) {
            bvh->intersect(ray);
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0).count();
    };

    // It is expected that BVH performs at O(log(N)) (branching factor=2).
    const double dt_a = measure_seconds(100);
    const double dt_b = measure_seconds(5000);
    const double ratio_theoretical = std::log2(5000.0 / 100.0);
    const double ratio = dt_b / dt_a;

    EXPECT_LT(ratio_theoretical / 2, ratio);
    EXPECT_GT(ratio_theoretical * 2, ratio);
}
