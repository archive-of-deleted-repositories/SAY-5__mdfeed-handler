#include "obs/histogram.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mdfeed::obs {

namespace {

int leading_zeros_u64(uint64_t v) {
    if (v == 0) return 64;
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_clzll(v);
#else
    int n = 0;
    while ((v & (1ull << 63)) == 0) {
        v <<= 1;
        ++n;
    }
    return n;
#endif
}

int floor_log2_u64(uint64_t v) {
    return 63 - leading_zeros_u64(v);
}

}  // namespace

Histogram::Histogram() : Histogram(1, 1'000'000'000ull, 3) {}

Histogram::Histogram(uint64_t min_value, uint64_t max_value, int significant_figures) {
    if (min_value < 1) min_value = 1;
    if (max_value <= 2 * min_value) max_value = 2 * min_value;
    if (significant_figures < 1) significant_figures = 1;
    if (significant_figures > 5) significant_figures = 5;

    min_ = min_value;
    max_ = max_value;
    sigfigs_ = significant_figures;

    // Compute the smallest sub-bucket count >= 2 * 10^sigfigs that is a
    // power of two. This is the HDR scheme.
    uint64_t largest_value_with_single_unit_resolution = 1;
    for (int i = 0; i < sigfigs_; ++i)
        largest_value_with_single_unit_resolution *= 10;
    largest_value_with_single_unit_resolution *= 2;

    int sub_bucket_count_magnitude = static_cast<int>(
        std::ceil(std::log2(static_cast<double>(largest_value_with_single_unit_resolution))));
    sub_bucket_half_bits_ = sub_bucket_count_magnitude - 1;
    sub_bucket_count_ = 1 << sub_bucket_count_magnitude;
    sub_bucket_half_count_ = sub_bucket_count_ / 2;

    unit_magnitude_ = floor_log2_u64(min_);
    if (unit_magnitude_ < 0) unit_magnitude_ = 0;

    // Determine bucket_count such that the largest representable value
    // covers max_value.
    int smallest_untrackable_magnitude = unit_magnitude_ + sub_bucket_count_magnitude;
    uint64_t smallest_untrackable_value = 1ull << smallest_untrackable_magnitude;
    int buckets_needed = 1;
    while (smallest_untrackable_value <= max_) {
        smallest_untrackable_value <<= 1;
        ++buckets_needed;
    }
    bucket_count_ = buckets_needed;

    leading_zero_count_base_ = 64 - unit_magnitude_ - sub_bucket_count_magnitude;

    std::size_t counts_size =
        static_cast<std::size_t>((bucket_count_ + 1) * sub_bucket_half_count_);
    counts_.assign(counts_size, 0);
}

uint64_t Histogram::bucket_index_(uint64_t value) const {
    uint64_t pow2ceiling = value | ((1ull << unit_magnitude_) - 1);
    int bucket = leading_zero_count_base_ - leading_zeros_u64(pow2ceiling);
    if (bucket < 0) bucket = 0;
    if (bucket > bucket_count_) bucket = bucket_count_;

    int sub_bucket_index = static_cast<int>(value >> (bucket + unit_magnitude_));
    int offset_in_bucket = sub_bucket_index - sub_bucket_half_count_;
    int bucket_base_index = (bucket + 1) * sub_bucket_half_count_;
    int idx = bucket_base_index + offset_in_bucket;
    if (idx < 0) idx = 0;
    if (static_cast<std::size_t>(idx) >= counts_.size()) {
        idx = static_cast<int>(counts_.size() - 1);
    }
    return static_cast<uint64_t>(idx);
}

uint64_t Histogram::bucket_low_value_(std::size_t idx) const {
    // Inverse of bucket_index_: return the smallest value that maps to idx.
    int bucket_base_index = sub_bucket_half_count_;
    int bucket = 0;
    while (bucket_base_index + sub_bucket_half_count_ <= static_cast<int>(idx)) {
        bucket_base_index += sub_bucket_half_count_;
        ++bucket;
    }
    int offset_in_bucket = static_cast<int>(idx) - bucket_base_index;
    int sub_bucket_index = offset_in_bucket + sub_bucket_half_count_;
    uint64_t v = static_cast<uint64_t>(sub_bucket_index) << (bucket + unit_magnitude_);
    return v;
}

void Histogram::record(uint64_t value) {
    if (value < min_) {
        ++underflow_count_;
        value = min_;
    }
    if (value > max_) {
        ++overflow_count_;
        value = max_;
    }
    uint64_t idx = bucket_index_(value);
    if (idx < counts_.size()) {
        counts_[idx] += 1;
        total_count_ += 1;
    }
}

uint64_t Histogram::value_at_percentile(double p) const {
    if (total_count_ == 0) return 0;
    if (p < 0.0) p = 0.0;
    if (p > 100.0) p = 100.0;

    // Use ceil so the percentile semantics are "at least p% of samples are
    // <= the returned value". This matches the canonical HDR behaviour.
    double count_at_percentile_d = (p / 100.0) * static_cast<double>(total_count_);
    uint64_t count_at_percentile = static_cast<uint64_t>(count_at_percentile_d + 0.5);
    if (count_at_percentile == 0) count_at_percentile = 1;
    if (count_at_percentile > total_count_) count_at_percentile = total_count_;

    uint64_t running = 0;
    for (std::size_t i = 0; i < counts_.size(); ++i) {
        running += counts_[i];
        if (running >= count_at_percentile) {
            // Return the high end of the bucket so we don't under-report.
            uint64_t low = bucket_low_value_(i);
            uint64_t next_low = (i + 1 < counts_.size()) ? bucket_low_value_(i + 1) : max_;
            uint64_t hi = (next_low > 0) ? (next_low - 1) : low;
            return hi;
        }
    }
    return max_;
}

}  // namespace mdfeed::obs
