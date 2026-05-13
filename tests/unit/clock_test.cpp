#include "obs/clock.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace mdfeed::obs;

TEST(Clock, NowIsNonZero) {
    EXPECT_GT(now_ns(), 0u);
}

TEST(Clock, IsMonotonic) {
    uint64_t prev = now_ns();
    for (int i = 0; i < 1000; ++i) {
        uint64_t cur = now_ns();
        ASSERT_GE(cur, prev);
        prev = cur;
    }
}

TEST(Clock, ResolutionIsBetterThanMicrosecond) {
    // We record distinct now_ns() returns across a sleep of 1us. The
    // claim "sub-microsecond granularity" is supported if at least one
    // pair of consecutive samples within a 1us window differs by less
    // than 1us. (Resolution, not precision.)
    bool saw_sub_us = false;
    uint64_t prev = now_ns();
    for (int i = 0; i < 10'000; ++i) {
        uint64_t cur = now_ns();
        if (cur != prev && (cur - prev) < 1000u) {
            saw_sub_us = true;
            break;
        }
        prev = cur;
    }
    EXPECT_TRUE(saw_sub_us);
}

TEST(Clock, RdtscFlagDoesNotCrash) {
    // We don't assert true or false; on CI runners with virtualized x86
    // the flag may go either way. The check is just that the function
    // returns without crashing.
    (void)rdtsc_invariant_supported();
}

TEST(Clock, SleepIsObserved) {
    auto t0 = now_ns();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto t1 = now_ns();
    EXPECT_GE(t1 - t0, 4'000'000u);  // at least 4ms; gives 1ms slop
}
