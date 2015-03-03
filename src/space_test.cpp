#include "space.h"

#include <array>
#include <cmath>
#include <random>

#include <boost/range/irange.hpp>
#include <gtest/gtest.h>

TEST(cross, NonDegenerate) {
    {
        const Eigen::Vector4f v0(1, 0, 0, 0);
        const Eigen::Vector4f v1(0, 1, 0, 0);
        const Eigen::Vector4f v2(0, 0, 1, 0);

        const Eigen::Vector4f v = pentatope::cross(v0, v1, v2);
        EXPECT_FLOAT_EQ(0, v0.dot(v));
        EXPECT_FLOAT_EQ(0, v1.dot(v));
        EXPECT_FLOAT_EQ(0, v2.dot(v));
    }
    {
        std::mt19937 rd(1);
        std::uniform_real_distribution<> distrib(-3, 3);
        // random testing
        for(const int i_sample : boost::irange(0, 100)) {
            std::array<Eigen::Vector4f, 3> vs;
            for(const int i : boost::irange(0, 3)) {
                vs[i] = Eigen::Vector4f(
                    distrib(rd),
                    distrib(rd),
                    distrib(rd),
                    distrib(rd));
            }
            // TODO: this test can be flaky when vs is degenerate.
            const Eigen::Vector4f v = pentatope::cross(vs[0], vs[1], vs[2]);
            EXPECT_GT(1e-3, std::abs(vs[0].dot(v)));
            EXPECT_GT(1e-3, std::abs(vs[1].dot(v)));
            EXPECT_GT(1e-3, std::abs(vs[2].dot(v)));
        }
    }
}
