#include "core/normalize_a.h"

#include <cstring>

namespace mdfeed::venue_a {

namespace {

template <typename T>
T read_le(const uint8_t* p) {
    T v{};
    std::memcpy(&v, p, sizeof(T));
    return v;
}

template <typename T>
void write_le(uint8_t* p, T v) {
    std::memcpy(p, &v, sizeof(T));
}

}  // namespace

std::optional<MdMessage> parse(const uint8_t* buf, std::size_t len, TsNs recv_ns, uint64_t seq) {
    if (buf == nullptr || len < kWireSize) {
        return std::nullopt;
    }

    const uint8_t raw_type = buf[0];
    const uint8_t type_lo = static_cast<uint8_t>(raw_type & 0x7F);
    const bool is_ask = (raw_type & 0x80) != 0;

    if (type_lo != static_cast<uint8_t>(MsgType::Quote) &&
        type_lo != static_cast<uint8_t>(MsgType::Cancel)) {
        return std::nullopt;
    }

    MdMessage m{};
    m.type = static_cast<MsgType>(type_lo);
    m.venue_id = read_le<uint16_t>(buf + 1);
    m.symbol_id = read_le<uint32_t>(buf + 3);
    m.price_ticks = read_le<int64_t>(buf + 7);
    m.qty = read_le<uint32_t>(buf + 15);
    m.ts_ns_venue = read_le<uint64_t>(buf + 19);
    m.side = is_ask ? Side::Ask : Side::Bid;
    m.ts_ns_recv = recv_ns;
    m.seq = seq;

    if (m.venue_id != kVenueA) {
        return std::nullopt;
    }
    return m;
}

std::size_t serialize(const MdMessage& msg, uint8_t* buf, std::size_t cap) {
    if (buf == nullptr || cap < kWireSize) {
        return 0;
    }
    uint8_t type_byte = static_cast<uint8_t>(msg.type);
    if (msg.side == Side::Ask) {
        type_byte = static_cast<uint8_t>(type_byte | 0x80);
    }
    buf[0] = type_byte;
    write_le<uint16_t>(buf + 1, msg.venue_id);
    write_le<uint32_t>(buf + 3, msg.symbol_id);
    write_le<int64_t>(buf + 7, msg.price_ticks);
    write_le<uint32_t>(buf + 15, msg.qty);
    write_le<uint64_t>(buf + 19, msg.ts_ns_venue);
    return kWireSize;
}

}  // namespace mdfeed::venue_a
