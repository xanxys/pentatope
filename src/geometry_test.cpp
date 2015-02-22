#include "geometry.h"

#include <cmath>

#include <gtest/gtest.h>

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
