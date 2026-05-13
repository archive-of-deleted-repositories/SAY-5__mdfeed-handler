# Architecture

```
   feed simulator (process)
   ┌────────────────────────┐
   │ deterministic xorshift │
   │ msg generator          │
   │   ├─ venue A binary    │       loopback UDP
   │   └─ venue B ASCII     │ ─────► 127.0.0.1:42424
   └────────────────────────┘
                                          │
                                          ▼
                              ┌────────────────────────┐
                              │ FeedHandler            │
                              │   recvfrom             │
                              │   disambiguate venue   │
                              │   normalize → MdMsg    │
                              │   gap-detect           │
                              │   BboBook.apply        │
                              │   Histogram.record     │
                              └────────────────────────┘
                                          │
                                          ▼
                              ┌──────────────────────┐
                              │ BBO state per symbol │
                              │ HDR latency hist     │
                              └──────────────────────┘
```

## Why BBO and not full depth

See [`docs/bbo-vs-depth.md`](docs/bbo-vs-depth.md). Short version: full
depth + matching is the subject of `SAY-5/orderbook-sim`. This repo is
the read-side wire-to-state companion.

## Latency methodology

See [`docs/latency-methodology.md`](docs/latency-methodology.md). Two
histograms:

- `wire_to_normalized_ns`, single-host measurement of parse + book
  update cost. Sub-microsecond range on commodity hardware.
- `venue_to_recv_ns`, captures the full UDP roundtrip including
  `sendto` and `recvfrom` syscall overhead. Microsecond range on the
  standard POSIX stack; this is the part that kernel-bypass would
  shrink. Not implemented here on purpose.

## HDR histogram bucket sizing

3 significant figures over `[1ns, 1s]`. Sub-bucket count is 2048 (the
smallest power of two at or above `2 * 10^3`). 25 powers-of-two cover the
1-second top of the range. Memory: roughly 200 KB per histogram; cheap
enough to keep two of them live for the whole run.

## Gap detection

Per-venue last-seen sequence numbers (`last_seq_a_`, `last_seq_b_`).
When a new message's seq isn't `last + 1`, we increment
`stats.gaps_detected`. **No gap fill** is performed; that is a venue-
protocol-level concern (TCP retransmission channels, recovery feeds,
snapshot replays). For real venues, the swap-in is a separate
`RecoveryClient` component sitting alongside `FeedHandler`.

## Why POSIX UDP and not kernel-bypass

- This is a study, not a production data center deployment. The
  point is honest measurement of the standard path so the cost of the
  kernel can be quantified.
- Hermetic CI on GitHub Actions over Ubuntu runners has no DPDK setup.
  POSIX UDP on `127.0.0.1` works without any privileges or hardware.
- Kernel-bypass is operationally heavy: hugepages, NIC binding, CPU
  pinning, polling threads burning 100% CPU. None of that belongs in a
  scaffold.

## What is deliberately not here

- No real venue gateway, no multicast group membership, no IGMP
- No FIX, ITCH, PITCH, MoldUDP, or any standardized protocol parser
- No full depth-by-price book
- No order entry
- No recovery / gap-fill protocol
- No DPDK, ef_vi, Solarflare TCPDirect, or any kernel-bypass
- No CPU pinning or NUMA-aware allocation
- No SIMD parse path for venue A (the parse is already aligned memcpy
  reads, which compilers handle well; explicit SIMD would be a
  micro-optimization without changing the architecture)
