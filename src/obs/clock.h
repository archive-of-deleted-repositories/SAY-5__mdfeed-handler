#pragma once

#include <cstdint>

#include "core/types.h"

namespace mdfeed::obs {

// Returns current CLOCK_MONOTONIC_RAW timestamp in ns. Monotonic across
// the process lifetime, unaffected by NTP slewing. Resolution is typically
// 1ns on modern Linux x86_64 / arm64; we don't claim better than that.
TsNs now_ns();

// Returns true if the platform supports the rdtsc fast path AND it has been
// gated as safe to use (invariant TSC). On unsupported platforms or in CI,
// we fall back to clock_gettime exclusively. The flag is informational; the
// actual now_ns() above always returns CLOCK_MONOTONIC_RAW. We do not ship
// the rdtsc fast path here because its calibration story is not portable
// across CI runners; the methodology doc explains the swap-in.
bool rdtsc_invariant_supported();

}  // namespace mdfeed::obs
