#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "core/types.h"

namespace mdfeed::venue_a {

// Venue A wire layout (little-endian, 27 bytes):
//   [0]    uint8  msg_type   (1 = Quote, 2 = Cancel)
//   [1..2] uint16 venue_id   (LE)
//   [3..6] uint32 symbol_id  (LE)
//   [7..14] int64 price_int (LE, ticks, kPriceScale = 1e6)
//   [15..18] uint32 qty     (LE)
//   [19..26] uint64 ts_ns   (LE, venue-stamped wall time)
//
// Side is encoded in the high bit of msg_type:
//   bit 7 = 0 -> Bid, bit 7 = 1 -> Ask. Low 7 bits are MsgType.
constexpr std::size_t kWireSize = 27;

// Returns parsed MdMessage on success, std::nullopt on malformed input.
// `recv_ns` is the locally-stamped receive time, copied into ts_ns_recv.
// `seq` is the per-venue sequence number tracked by the caller.
std::optional<MdMessage> parse(const uint8_t* buf, std::size_t len, TsNs recv_ns, uint64_t seq);

// Serialize an MdMessage back onto the wire (for the feed simulator).
// Returns number of bytes written, or 0 on failure (buffer too small).
std::size_t serialize(const MdMessage& msg, uint8_t* buf, std::size_t cap);

}  // namespace mdfeed::venue_a
