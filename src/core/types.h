#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace mdfeed {

using SymbolId = uint32_t;
using VenueId = uint16_t;
using Price = int64_t;  // integer ticks; scale documented per venue
using Qty = uint32_t;
using TsNs = uint64_t;

enum class Side : uint8_t {
    Bid = 0,
    Ask = 1,
};

enum class MsgType : uint8_t {
    Quote = 1,
    Cancel = 2,
};

// Unified internal message. Both wire formats normalize into this shape.
struct MdMessage {
    MsgType type;
    VenueId venue_id;
    SymbolId symbol_id;
    Side side;
    Price price_ticks;
    Qty qty;
    TsNs ts_ns_venue;  // venue-stamped timestamp from the wire
    TsNs ts_ns_recv;   // local CLOCK_MONOTONIC_RAW at recvfrom return
    uint64_t seq;      // per-venue sequence number (0 if not provided)
};

// Per-symbol top-of-book state. BBO only; this is not a depth book.
struct BboState {
    Price bid_price = 0;
    Qty bid_qty = 0;
    Price ask_price = 0;
    Qty ask_qty = 0;
    TsNs last_recv_ns = 0;
    uint64_t last_seq = 0;
    bool has_bid = false;
    bool has_ask = false;
};

constexpr VenueId kVenueA = 1;
constexpr VenueId kVenueB = 2;

// Price scale: 6 decimal digits (1.000000 == 1'000'000 ticks).
// Picked to fit equity-style price precision in a fixed integer without
// the dynamic ranges that would require floats.
constexpr int64_t kPriceScale = 1'000'000;

}  // namespace mdfeed
