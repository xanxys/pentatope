#include "geometry.h"

#include <cmath>
#include <random>

#include <boost/range/irange.hpp>
#include <gtest/gtest.h>

#include <arbitrary_test.h>


TEST(AABB, IntersectionOutGoing) {
    std::mt19937 rg;

    {
        const pentatope::AABB aabb(
            Eigen::Vector4f(0, 0, 0, 0), Eigen::Vector4f(1, 1, 1, 1));
        const pentatope::Ray ray(
            Eigen::Vector4f(0.5, 0.5, 0.5, 0.5),
            Eigen::Vector4f(1, 0, 0, 0));
        const auto isect = aabb.intersect(ray);
        EXPECT_TRUE(isect);
        EXPECT_EQ(
            Eigen::Vector4f(1, 0.5, 0.5, 0.5),
            isect->pos());
        EXPECT_EQ(
            Eigen::Vector4f(1, 0, 0, 0), // outward pointing
            isect->normal());
    }

    // A case encountered in debugging #7
    {
        const pentatope::AABB aabb(
            Eigen::Vector4f(-200, -200, -200, -200),
            Eigen::Vector4f(200, 200, 200, 200));
        const pentatope::Ray ray(
            Eigen::Vector4f(-16.9445, 3.52139, 83.1682, 96.4669),
            Eigen::Vector4f(-0.536506, 0.13881, -0.466949, -0.689095));
        const auto isect = aabb.intersect(ray);
        EXPECT_TRUE(isect);
    }

    // Check arbitrary rays whose origin is inside an AABB.
    // All of them must intersect with AABBs.
    for(const int i : boost::irange(0, 100)) {
        const Eigen::Vector4f vmin(
            std::uniform_real_distribution<float>(-100, 100)(rg),
            std::uniform_real_distribution<float>(-100, 100)(rg),
            std::uniform_real_distribution<float>(-100, 100)(rg),
            std::uniform_real_distribution<float>(-100, 100)(rg));
        const Eigen::Vector4f size(
            std::uniform_real_distribution<float>(0, 100)(rg),
            std::uniform_real_distribution<float>(0, 100)(rg),
            std::uniform_real_distribution<float>(0, 100)(rg),
            std::uniform_real_distribution<float>(0, 100)(rg));

        const pentatope::AABB aabb(vmin, vmin + size);
        const Eigen::Vector4f org =
            vmin + size.cwiseProduct(Eigen::Vector4f(
                std::uniform_real_distribution<float>(0, 1)(rg),
                std::uniform_real_distribution<float>(0, 1)(rg),
                std::uniform_real_distribution<float>(0, 1)(rg),
                std::uniform_real_distribution<float>(0, 1)(rg)));
        Eigen::Vector4f dir(
            std::uniform_real_distribution<float>(-1, 1)(rg),
            std::uniform_real_distribution<float>(-1, 1)(rg),
            std::uniform_real_distribution<float>(-1, 1)(rg),
            std::uniform_real_distribution<float>(-1, 1)(rg));
        dir.normalize();
        const pentatope::Ray ray(org, dir);
        EXPECT_TRUE(aabb.intersect(ray));
    }
}

TEST(OBB, IntersectionOutGoing) {
    const pentatope::OBB obb(
        pentatope::Pose(),
        Eigen::Vector4f(1, 1, 1, 1));

    {
        const pentatope::Ray ray(
            Eigen::Vector4f(0, 0, 0, 0),
            Eigen::Vector4f(1, 0, 0, 0));
        const auto isect = obb.intersect(ray);
        EXPECT_TRUE(isect);
        EXPECT_EQ(
            Eigen::Vector4f(0.5, 0, 0, 0),
            isect->pos());
        EXPECT_EQ(
            Eigen::Vector4f(1, 0, 0, 0), // outward pointing
            isect->normal());
    }
}

TEST(OBB, IntersectionAtOriginXDir) {
    const pentatope::OBB obb(pentatope::Pose(), Eigen::Vector4f(1, 1, 1, 1));

    // Should hit negative surface from outside.
    {
        const pentatope::Ray rx_pos(
            Eigen::Vector4f(-2, 0, 0, 0),
            Eigen::Vector4f(1, 0, 0, 0));

        const auto rx_pos_isect = obb.intersect(rx_pos);
        EXPECT_TRUE(rx_pos_isect);
        EXPECT_EQ(
            Eigen::Vector4f(-0.5, 0, 0, 0),
            rx_pos_isect->pos());
        EXPECT_EQ(
            Eigen::Vector4f(-1, 0, 0, 0),
            rx_pos_isect->normal());  // outward pointing
    }

    // Should hit positive surface from inside.
    {
        const pentatope::Ray rx_pos(
            Eigen::Vector4f(0, 0, 0, 0),
            Eigen::Vector4f(1, 0, 0, 0));

        const auto rx_pos_isect = obb.intersect(rx_pos);
        EXPECT_TRUE(rx_pos_isect);
        EXPECT_EQ(
            Eigen::Vector4f(0.5, 0, 0, 0),
            rx_pos_isect->pos());
        EXPECT_EQ(
            Eigen::Vector4f(1, 0, 0, 0),
            rx_pos_isect->normal());  // outward pointing
    }

    // Should not hit anything.
    {
        const pentatope::Ray rx_pos(
            Eigen::Vector4f(-3, 1.1, 0, 0),
            Eigen::Vector4f(1, 0, 0, 0));

        const auto rx_pos_isect = obb.intersect(rx_pos);
        EXPECT_FALSE(rx_pos_isect);
    }
}


TEST(OBB, IntersectionTranslated) {
    // Occupies
    // z: [0, 1]
    // x,y,w: [-0.5, 0.5]
    const pentatope::OBB obb(
        pentatope::Pose(
            Eigen::Matrix4f::Identity(),
            Eigen::Vector4f(0, 0, 0.5, 0)),
        Eigen::Vector4f(1, 1, 1, 1));

    // Should hit negative surface from outside.
    {
        const pentatope::Ray rx_pos(
            Eigen::Vector4f(-2, 0, 0.5, 0),
            Eigen::Vector4f(1, 0, 0, 0));

        const auto rx_pos_isect = obb.intersect(rx_pos);
        EXPECT_TRUE(rx_pos_isect);
        EXPECT_EQ(
            Eigen::Vector4f(-0.5, 0, 0.5, 0),
            rx_pos_isect->pos());
        EXPECT_EQ(
            Eigen::Vector4f(-1, 0, 0, 0),
            rx_pos_isect->normal());  // outward pointing
    }

    // Should not hit anything.
    {
        const pentatope::Ray rx_pos(
            Eigen::Vector4f(-3, 0, -0.1, 0),
            Eigen::Vector4f(1, 0, 0, 0));

        const auto rx_pos_isect = obb.intersect(rx_pos);
        EXPECT_FALSE(rx_pos_isect);
    }
}


TEST(Disc, BoundsIsCorrect) {
    std::mt19937 rg;

    for(const int i : boost::irange(0, 100)) {
        const auto ray = arbitraryRay(rg);

        Eigen::Vector4f center(
            std::uniform_real_distribution<float>(-10, 10)(rg),
            std::uniform_real_distribution<float>(-10, 10)(rg),
            std::uniform_real_distribution<float>(-10, 10)(rg),
            std::uniform_real_distribution<float>(-10, 10)(rg));

        Eigen::Vector4f normal(
            std::uniform_real_distribution<float>(-1, 1)(rg),
            std::uniform_real_distribution<float>(-1, 1)(rg),
            std::uniform_real_distribution<float>(-1, 1)(rg),
            std::uniform_real_distribution<float>(-1, 1)(rg));
        normal.normalize();

        const float radius = std::uniform_real_distribution<float>(0.1, 100)(rg);

        const pentatope::Disc disc(center, normal, radius);
        const auto isect = disc.intersect(ray);
        if(isect) {
            EXPECT_TRUE(disc.bounds().contains(isect->pos()));
        }
    }
}
