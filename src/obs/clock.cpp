#include "obs/clock.h"

#include <ctime>

#if defined(__x86_64__) || defined(_M_X64)
#include <cstring>
#if defined(__GNUC__) || defined(__clang__)
#include <cpuid.h>
#endif
#endif

namespace mdfeed::obs {

TsNs now_ns() {
#if defined(CLOCK_MONOTONIC_RAW)
    constexpr clockid_t kClock = CLOCK_MONOTONIC_RAW;
#else
    constexpr clockid_t kClock = CLOCK_MONOTONIC;
#endif
    timespec ts{};
    clock_gettime(kClock, &ts);
    return static_cast<TsNs>(ts.tv_sec) * 1'000'000'000ull + static_cast<TsNs>(ts.tv_nsec);
}

bool rdtsc_invariant_supported() {
#if (defined(__x86_64__) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (!__get_cpuid_max(0x80000000u, nullptr)) return false;
    if (!__get_cpuid(0x80000007u, &eax, &ebx, &ecx, &edx)) return false;
    // Invariant TSC = EDX bit 8.
    return (edx & (1u << 8)) != 0;
#else
    return false;
#endif
}

}  // namespace mdfeed::obs
