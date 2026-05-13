#include "obs/histogram.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

using namespace mdfeed::obs;

TEST(Histogram, EmptyReturnsZero) {
    Histogram h;
    EXPECT_EQ(h.value_at_percentile(50.0), 0u);
    EXPECT_EQ(h.count(), 0u);
}

TEST(Histogram, SingleSampleAllPercentiles) {
    Histogram h;
    h.record(1000);
    EXPECT_EQ(h.count(), 1u);
    // 3-sigfig resolution: must be within ~0.1% of recorded value.
    auto p = h.value_at_percentile(50.0);
    EXPECT_GE(p, 999u);
    EXPECT_LE(p, 1010u);
}

TEST(Histogram, MonotonicPercentiles) {
    Histogram h;
    std::mt19937_64 rng(12345);
    for (int i = 0; i < 100'000; ++i) {
        // Heavy-tailed
        uint64_t v = 100ull + (rng() % 10'000ull);
        if ((rng() & 0xFF) == 0) v += rng() % 100'000ull;
        h.record(v);
    }
    auto p50 = h.value_at_percentile(50.0);
    auto p95 = h.value_at_percentile(95.0);
    auto p99 = h.value_at_percentile(99.0);
    auto p999 = h.value_at_percentile(99.9);
    EXPECT_LE(p50, p95);
    EXPECT_LE(p95, p99);
    EXPECT_LE(p99, p999);
}

TEST(Histogram, MatchesSortedArrayWithinResolution) {
    Histogram h(1, 1'000'000ull, 3);
    std::vector<uint64_t> samples;
    std::mt19937_64 rng(42);
    for (int i = 0; i < 50'000; ++i) {
        uint64_t v = 10 + (rng() % 200'000ull);
        h.record(v);
        samples.push_back(v);
    }
    std::sort(samples.begin(), samples.end());

    for (double pct : {50.0, 90.0, 95.0, 99.0}) {
        std::size_t idx =
            static_cast<std::size_t>((pct / 100.0) * static_cast<double>(samples.size()) + 0.5);
        if (idx >= samples.size()) idx = samples.size() - 1;
        uint64_t ref = samples[idx];
        uint64_t got = h.value_at_percentile(pct);
        // 3 sigfigs => +/- 0.5% tolerance, but allow a touch more for
        // bucket high-end semantics.
        double rel = (ref == 0) ? 0.0
                                : std::abs(static_cast<double>(got) - static_cast<double>(ref)) /
                                      static_cast<double>(ref);
        EXPECT_LT(rel, 0.02) << "pct=" << pct << " ref=" << ref << " got=" << got;
    }
}

TEST(Histogram, ClampsBelowMin) {
    Histogram h(100, 1'000'000, 3);
    h.record(50);
    EXPECT_EQ(h.underflow(), 1u);
    EXPECT_EQ(h.count(), 1u);
    EXPECT_GE(h.value_at_percentile(50.0), 100u);
}

TEST(Histogram, ClampsAboveMax) {
    Histogram h(1, 1000, 3);
    h.record(5000);
    EXPECT_EQ(h.overflow(), 1u);
    EXPECT_EQ(h.count(), 1u);
}

TEST(Histogram, WideDynamicRange) {
    Histogram h(1, 1'000'000'000ull, 3);
    h.record(1);
    h.record(1'000);
    h.record(1'000'000);
    h.record(1'000'000'000);
    EXPECT_EQ(h.count(), 4u);
    EXPECT_EQ(h.overflow(), 0u);
}

TEST(Histogram, P100ReturnsMaxRecorded) {
    Histogram h(1, 1'000'000ull, 3);
    h.record(10);
    h.record(100);
    h.record(1000);
    auto p100 = h.value_at_percentile(100.0);
    EXPECT_GE(p100, 1000u);
}
