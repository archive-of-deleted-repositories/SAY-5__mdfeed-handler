#include "sim/feed_simulator.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <thread>

#include "core/normalize_a.h"
#include "core/normalize_b.h"
#include "core/types.h"
#include "net/udp_socket.h"
#include "obs/clock.h"

namespace mdfeed::sim {

namespace {

// xorshift64*; deterministic given the seed.
struct Rng {
    uint64_t s;
    uint64_t next() {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        return s * 2685821657736338717ull;
    }
    uint32_t u32() { return static_cast<uint32_t>(next()); }
    uint32_t range(uint32_t lo, uint32_t hi) { return lo + (u32() % (hi - lo + 1)); }
};

const std::array<const char*, 8> kSymbols = {
    "AAPL", "MSFT", "GOOG", "AMZN", "META", "NVDA", "TSLA", "NFLX",
};

}  // namespace

int run(const SimConfig& cfg) {
    net::UdpSocket sock;
    if (sock.fd() < 0) return -1;
    int rc = sock.connect(cfg.dest_host, cfg.dest_port);
    if (rc != 0) return rc;

    Rng rng{cfg.seed};
    int num_syms = cfg.num_symbols;
    if (num_syms < 1) num_syms = 1;
    if (num_syms > static_cast<int>(kSymbols.size())) num_syms = static_cast<int>(kSymbols.size());

    // Per-symbol base prices in ticks (kPriceScale = 1e6). Random within
    // a sane range so jitter shows up.
    std::array<int64_t, 8> base_price{};
    for (int i = 0; i < num_syms; ++i) {
        base_price[i] = static_cast<int64_t>(rng.range(50, 500)) * kPriceScale;
    }

    const TsNs t_start = obs::now_ns();
    const uint64_t ns_per_msg =
        (cfg.target_rate_hz > 0) ? (1'000'000'000ull / cfg.target_rate_hz) : 0;

    std::array<uint8_t, 256> buf{};
    uint64_t sent = 0;

    while (sent < cfg.total_messages) {
        int sym_idx = static_cast<int>(rng.next() % num_syms);
        bool use_b = (rng.next() & 1ull) == 0;

        // Walk price up or down by 1..5 ticks.
        int64_t jitter = static_cast<int64_t>(rng.range(1, 5));
        bool down = (rng.next() & 2ull) != 0;
        if (down) jitter = -jitter;
        base_price[sym_idx] += jitter;
        if (base_price[sym_idx] < kPriceScale) base_price[sym_idx] = kPriceScale;

        MdMessage m{};
        m.type = MsgType::Quote;
        m.symbol_id = venue_b::hash_symbol(kSymbols[sym_idx]);
        m.side = (rng.next() & 1ull) ? Side::Bid : Side::Ask;
        m.price_ticks = base_price[sym_idx];
        m.qty = rng.range(100, 1000);
        m.ts_ns_venue = obs::now_ns();

        ssize_t n_out = 0;
        if (use_b) {
            m.venue_id = kVenueB;
            std::size_t bn = venue_b::serialize(m, kSymbols[sym_idx], buf.data(), buf.size());
            if (bn == 0) continue;
            n_out = sock.send(buf.data(), bn);
        } else {
            m.venue_id = kVenueA;
            // Venue A wire encodes symbol as an id directly. Use the same
            // FNV-1a hash so the two venues agree on symbol identity.
            std::size_t bn = venue_a::serialize(m, buf.data(), buf.size());
            if (bn == 0) continue;
            n_out = sock.send(buf.data(), bn);
        }
        if (n_out < 0) {
            // Loopback ENOBUFS under burst: brief pause and retry.
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            continue;
        }
        ++sent;

        if (ns_per_msg > 0) {
            const TsNs target = t_start + sent * ns_per_msg;
            const TsNs now = obs::now_ns();
            if (target > now) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(target - now));
            }
        }
    }
    return 0;
}

}  // namespace mdfeed::sim
