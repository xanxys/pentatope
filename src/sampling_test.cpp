#include "sampling.h"

#include <cmath>

#include <gtest/gtest.h>

TEST(Sampler, splitGeneratesDifferentSequence) {
    std::uniform_int_distribution<uint64_t> prob_u64(
        std::numeric_limits<uint64_t>::min(),
        std::numeric_limits<uint64_t>::max());

    pentatope::Sampler parent;
    pentatope::Sampler parent_copy = parent;
    const uint64_t v_parent_prev = prob_u64(parent_copy.gen);

    auto children = parent.split(2);
    EXPECT_EQ(2, children.size());

    const uint64_t v_parent = prob_u64(parent.gen);
    EXPECT_NE(v_parent, v_parent_prev);
    const uint64_t v_c0 = prob_u64(children[0].gen);
    const uint64_t v_c1 = prob_u64(children[1].gen);
    EXPECT_NE(v_parent, v_c0);
    EXPECT_NE(v_parent, v_c1);
    EXPECT_NE(v_c0, v_c1);
}
