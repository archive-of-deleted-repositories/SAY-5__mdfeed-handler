#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "core/types.h"

namespace mdfeed::venue_b {

// Venue B wire layout (pipe-delimited ASCII, newline terminated):
//   V|VENUE_B|SYM=AAPL|SIDE=B|PX=192.34|QTY=100|TS=1715000000123456789\n
//
// Fields after the leading "V|VENUE_B":
//   SYM=<token>     symbol ticker, looked up to symbol_id by caller
//   SIDE=B|A        bid/ask
//   PX=<float>      decimal price, scaled to kPriceScale ticks
//   QTY=<uint>      quantity
//   TS=<uint64>     venue timestamp in ns
//
// The caller owns symbol-table lookup; for the study we hash the SYM token
// directly into a 32-bit id (FNV-1a) so the parser is self-contained.
std::optional<MdMessage> parse(std::string_view line, TsNs recv_ns, uint64_t seq);

// Serializer used by the feed simulator. Returns bytes written, or 0 if
// the buffer is too small. Symbol is provided as a ticker string.
std::size_t serialize(const MdMessage& msg, std::string_view sym_ticker, uint8_t* buf,
                      std::size_t cap);

// FNV-1a 32-bit hash. Exposed so simulator + parser agree on symbol ids.
SymbolId hash_symbol(std::string_view s);

}  // namespace mdfeed::venue_b
