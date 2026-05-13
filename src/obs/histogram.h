#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mdfeed::obs {

// Compact HDR-style log-linear histogram. Bucketing is power-of-two with
// `sub_bucket_count` linear sub-buckets per power-of-two, same scheme as
// the canonical HDR Histogram, sized for 3 significant figures by default
// across [1ns, 1s].
//
// Memory cost at 3 sigfigs over that range is ~30 KiB, fine for in-process.
class Histogram {
public:
    Histogram();  // default: 3 sigfigs, range [1ns, 1s]
    Histogram(uint64_t min_value, uint64_t max_value, int significant_figures);

    // Record a value. Out-of-range values are clamped to min/max; the
    // counter is still incremented. We log overflow counts separately so
    // the caller can detect a misconfigured range.
    void record(uint64_t value);

    // Total samples recorded.
    uint64_t count() const { return total_count_; }

    // Number of samples that fell above max_value (clamped down).
    uint64_t overflow() const { return overflow_count_; }

    // Number of samples that fell below min_value (clamped up).
    uint64_t underflow() const { return underflow_count_; }

    // Returns the value at the given percentile in [0.0, 100.0].
    // Returns 0 if no samples recorded.
    uint64_t value_at_percentile(double p) const;

    uint64_t min_value() const { return min_; }
    uint64_t max_value() const { return max_; }

private:
    uint64_t bucket_index_(uint64_t value) const;
    uint64_t value_from_index_(std::size_t idx) const;
    uint64_t bucket_low_value_(std::size_t idx) const;

    uint64_t min_ = 1;
    uint64_t max_ = 1'000'000'000ull;
    int sigfigs_ = 3;
    int sub_bucket_count_ = 0;
    int sub_bucket_half_count_ = 0;
    int sub_bucket_half_bits_ = 0;  // log2(sub_bucket_half_count_)
    int unit_magnitude_ = 0;
    int bucket_count_ = 0;
    int leading_zero_count_base_ = 0;
    std::vector<uint64_t> counts_;
    uint64_t total_count_ = 0;
    uint64_t overflow_count_ = 0;
    uint64_t underflow_count_ = 0;
};

}  // namespace mdfeed::obs
