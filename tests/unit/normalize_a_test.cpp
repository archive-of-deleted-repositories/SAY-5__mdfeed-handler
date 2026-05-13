#include "core/normalize_a.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>

#include "core/types.h"

using namespace mdfeed;

namespace {

std::array<uint8_t, venue_a::kWireSize> make_wire(MsgType t, Side side, uint32_t sym, int64_t px,
                                                  uint32_t qty, uint64_t ts) {
    std::array<uint8_t, venue_a::kWireSize> b{};
    uint8_t tb = static_cast<uint8_t>(t);
    if (side == Side::Ask) tb = static_cast<uint8_t>(tb | 0x80);
    b[0] = tb;
    uint16_t v = kVenueA;
    std::memcpy(b.data() + 1, &v, sizeof(v));
    std::memcpy(b.data() + 3, &sym, sizeof(sym));
    std::memcpy(b.data() + 7, &px, sizeof(px));
    std::memcpy(b.data() + 15, &qty, sizeof(qty));
    std::memcpy(b.data() + 19, &ts, sizeof(ts));
    return b;
}

}  // namespace

TEST(NormalizeA, ValidBidQuote) {
    auto w = make_wire(MsgType::Quote, Side::Bid, 42, 19234'000000LL, 100, 1715000000000000000ull);
    auto m = venue_a::parse(w.data(), w.size(), 7, 1);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->type, MsgType::Quote);
    EXPECT_EQ(m->side, Side::Bid);
    EXPECT_EQ(m->venue_id, kVenueA);
    EXPECT_EQ(m->symbol_id, 42u);
    EXPECT_EQ(m->price_ticks, 19234'000000LL);
    EXPECT_EQ(m->qty, 100u);
    EXPECT_EQ(m->ts_ns_venue, 1715000000000000000ull);
    EXPECT_EQ(m->ts_ns_recv, 7u);
    EXPECT_EQ(m->seq, 1u);
}

TEST(NormalizeA, ValidAskQuoteHighBit) {
    auto w = make_wire(MsgType::Quote, Side::Ask, 99, 100'000000LL, 50, 1ull);
    auto m = venue_a::parse(w.data(), w.size(), 0, 0);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->side, Side::Ask);
}

TEST(NormalizeA, CancelTypeAccepted) {
    auto w = make_wire(MsgType::Cancel, Side::Bid, 1, 0, 0, 0);
    auto m = venue_a::parse(w.data(), w.size(), 0, 0);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->type, MsgType::Cancel);
}

TEST(NormalizeA, TruncatedReturnsNullopt) {
    auto w = make_wire(MsgType::Quote, Side::Bid, 1, 1, 1, 1);
    for (std::size_t i = 0; i < venue_a::kWireSize; ++i) {
        auto m = venue_a::parse(w.data(), i, 0, 0);
        EXPECT_FALSE(m.has_value()) << "len=" << i;
    }
}

TEST(NormalizeA, NullBufferRejected) {
    auto m = venue_a::parse(nullptr, 27, 0, 0);
    EXPECT_FALSE(m.has_value());
}

TEST(NormalizeA, WrongVenueIdRejected) {
    auto w = make_wire(MsgType::Quote, Side::Bid, 1, 1, 1, 1);
    uint16_t bad = 0xBEEF;
    std::memcpy(w.data() + 1, &bad, sizeof(bad));
    auto m = venue_a::parse(w.data(), w.size(), 0, 0);
    EXPECT_FALSE(m.has_value());
}

TEST(NormalizeA, UnknownTypeRejected) {
    auto w = make_wire(MsgType::Quote, Side::Bid, 1, 1, 1, 1);
    w[0] = 0x55;  // not Quote(1) or Cancel(2), low 7 bits = 0x55
    auto m = venue_a::parse(w.data(), w.size(), 0, 0);
    EXPECT_FALSE(m.has_value());
}

TEST(NormalizeA, SerializeRoundTrip) {
    MdMessage in{};
    in.type = MsgType::Quote;
    in.venue_id = kVenueA;
    in.symbol_id = 0xCAFEBABE;
    in.side = Side::Ask;
    in.price_ticks = -42'500000LL;
    in.qty = 12345;
    in.ts_ns_venue = 999'999'999'999'999'999ull;
    std::array<uint8_t, 64> buf{};
    std::size_t n = venue_a::serialize(in, buf.data(), buf.size());
    ASSERT_EQ(n, venue_a::kWireSize);
    auto out = venue_a::parse(buf.data(), n, 0, 0);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->venue_id, in.venue_id);
    EXPECT_EQ(out->symbol_id, in.symbol_id);
    EXPECT_EQ(out->side, in.side);
    EXPECT_EQ(out->price_ticks, in.price_ticks);
    EXPECT_EQ(out->qty, in.qty);
    EXPECT_EQ(out->ts_ns_venue, in.ts_ns_venue);
}

TEST(NormalizeA, SerializeTooSmallBuffer) {
    MdMessage in{};
    in.type = MsgType::Quote;
    in.venue_id = kVenueA;
    in.side = Side::Bid;
    std::array<uint8_t, 10> buf{};
    EXPECT_EQ(venue_a::serialize(in, buf.data(), buf.size()), 0u);
}
