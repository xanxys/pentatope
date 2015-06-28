#include "image_tile.h"

#include <cmath>

#include <boost/range/irange.hpp>
#include <gtest/gtest.h>


TEST(decomposeFloat, ZeroBecomeSmallest) {
	// 0 is not representable by our floating point,
	// but it should be something reasonable. (== the smallest value)
	const auto m_e = pentatope::decomposeFloat(0);
	EXPECT_EQ(0, m_e.first);
	EXPECT_EQ(0, m_e.second);
}

TEST(decomposeFloat, Exponent) {
	{
		// 1.0 * 2^0
		// mantissa = 0
		// exponent = 127
		const auto m_e = pentatope::decomposeFloat(1.0);
		EXPECT_EQ(0, m_e.first);
		EXPECT_EQ(127, m_e.second);
	}
	{
		// 0.5 = 1.0 * 2^(-1)
		const auto m_e = pentatope::decomposeFloat(0.5);
		EXPECT_EQ(0, m_e.first);
		EXPECT_EQ(126, m_e.second);
	}
}

TEST(decomposeFloat, Mantissa) {
	{
		// 1.5 * 2^0
		// mantissa = (1.5 - 1) * 256 = 128
		// exponent = 127
		const auto m_e = pentatope::decomposeFloat(1.5);
		EXPECT_EQ(128, m_e.first);
		EXPECT_EQ(127, m_e.second);
	}
}
