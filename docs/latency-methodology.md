# Latency Measurement Methodology

Two latencies are tracked per message.

## `wire_to_normalized_ns`

Time from "`recvfrom` returns" to "MdMessage fully populated, BBO update
applied, latency recorded". Measured as:

```
t0 = clock_gettime(CLOCK_MONOTONIC_RAW)     // immediately after recvfrom
... parse + bbo update ...
t1 = clock_gettime(CLOCK_MONOTONIC_RAW)
wire_to_normalized_ns = t1 - t0
```

This is a single-host, single-clock-domain measurement. No NTP slewing
affects it (`CLOCK_MONOTONIC_RAW`). Resolution on Linux x86_64 and arm64
is typically 1ns; we don't claim better than the kernel reports.

## `venue_to_recv_ns`

```
venue_to_recv_ns = ts_ns_recv - ts_ns_venue
```

In production this would measure venue-to-handler wire latency and
require PTP or hardware timestamping for clock alignment. In this study
the venue is a loopback process on the same host, so both timestamps
come from the same `CLOCK_MONOTONIC_RAW` and the measurement is
meaningful for the loopback path itself (it includes UDP send + recv
syscall overhead end-to-end).

When the simulator stamps `ts_ns_venue` *before* the `sendto`, the
measurement captures: simulator-side send overhead, kernel UDP queueing,
loopback NIC traversal, and the consumer's recvfrom return. That's why
P50 is in the microsecond range, not nanoseconds, that's the cost of
the standard POSIX UDP path.

## TSC fast path (not enabled by default)

`obs::clock::rdtsc_invariant_supported()` detects whether the CPU
advertises invariant TSC via CPUID 0x80000007. The check is exposed but
the histogram and feed handler use `clock_gettime` exclusively. Reasons:

- Calibration story is not portable across CI runners (no fixed TSC
  frequency MSR access, no warm-up loop budget in hermetic CI).
- VDSO `clock_gettime` on modern Linux is already a userspace read of
  the kernel-managed timekeeping page and is typically ~20-30 ns.

To enable a TSC fast path in a deployment, add an `obs::clock::tsc_ns()`
that captures `__rdtsc()` and converts via a measured ns-per-tick ratio
established at process start, and gate it on `rdtsc_invariant_supported()`.

## HDR histogram parameters

`obs::Histogram` uses the standard HDR log-linear bucketing:

- Range: `[1ns, 1s]` = `[1, 1_000_000_000]`
- Significant figures: 3 (relative error <= 0.1% per bucket)
- Sub-bucket count: 2048 (smallest power-of-two >= 2 * 10^3)
- Bucket count: 25 powers-of-two needed to cover 1s

Memory footprint: ~`(buckets+1) * 1024` uint64 counters = roughly 200 KB.
Underflow / overflow are counted separately so a clamp is visible.

## Percentile semantics

`value_at_percentile(p)` returns the high end of the bucket that contains
the `ceil(p% * total)`th sample. This matches the canonical HDR
implementation's behaviour and means reported percentiles never
under-report.

## What the numbers in `bench/results/bench_local.json` represent

The committed bench is 200,000 messages over loopback at a target rate
of 50,000 msgs/sec on the developer's local machine (Apple Silicon,
macOS). It is **not** a claim about production hardware or network
hardware. It is an honest baseline showing two things:

1. `wire_to_normalized_ns` is in the low-hundreds-of-nanoseconds range
   because the work is parse + map update + atomic counters; that part
   is fast and predictable.
2. `venue_to_recv_ns` is in the **microseconds** range, which is the
   true cost of `sendto` + loopback + `recvfrom` on a stock POSIX
   stack. Production deployments that need sub-microsecond on this leg
   use kernel-bypass (DPDK, ef_vi, ...), which is explicitly out of
   scope here.
