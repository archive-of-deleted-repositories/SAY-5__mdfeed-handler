#include "core/normalize_b.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include "core/types.h"

using namespace mdfeed;

TEST(NormalizeB, ValidLine) {
    std::string_view line = "V|VENUE_B|SYM=AAPL|SIDE=B|PX=192.34|QTY=100|TS=1715000000123456789\n";
    auto m = venue_b::parse(line, 11, 5);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->venue_id, kVenueB);
    EXPECT_EQ(m->symbol_id, venue_b::hash_symbol("AAPL"));
    EXPECT_EQ(m->side, Side::Bid);
    EXPECT_EQ(m->price_ticks, 192'340000LL);
    EXPECT_EQ(m->qty, 100u);
    EXPECT_EQ(m->ts_ns_venue, 1715000000123456789ull);
    EXPECT_EQ(m->ts_ns_recv, 11u);
    EXPECT_EQ(m->seq, 5u);
}

TEST(NormalizeB, AskSide) {
    std::string_view line = "V|VENUE_B|SYM=MSFT|SIDE=A|PX=300.00|QTY=10|TS=1\n";
    auto m = venue_b::parse(line, 0, 0);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->side, Side::Ask);
    EXPECT_EQ(m->price_ticks, 300'000000LL);
}

TEST(NormalizeB, NoTrailingNewline) {
    std::string_view line = "V|VENUE_B|SYM=GOOG|SIDE=B|PX=1.5|QTY=1|TS=1";
    auto m = venue_b::parse(line, 0, 0);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->price_ticks, 1'500000LL);
}

TEST(NormalizeB, MissingField) {
    std::string_view line = "V|VENUE_B|SYM=A|SIDE=B|PX=1|QTY=1";
    auto m = venue_b::parse(line, 0, 0);
    EXPECT_FALSE(m.has_value());
}

TEST(NormalizeB, WrongPrefix) {
    std::string_view line = "X|VENUE_C|SYM=A|SIDE=B|PX=1|QTY=1|TS=1";
    EXPECT_FALSE(venue_b::parse(line, 0, 0).has_value());
}

TEST(NormalizeB, BadSide) {
    std::string_view line = "V|VENUE_B|SYM=A|SIDE=X|PX=1|QTY=1|TS=1";
    EXPECT_FALSE(venue_b::parse(line, 0, 0).has_value());
}

TEST(NormalizeB, BadPrice) {
    std::string_view line = "V|VENUE_B|SYM=A|SIDE=B|PX=not-a-num|QTY=1|TS=1";
    EXPECT_FALSE(venue_b::parse(line, 0, 0).has_value());
}

TEST(NormalizeB, NegativePrice) {
    std::string_view line = "V|VENUE_B|SYM=A|SIDE=B|PX=-1.25|QTY=1|TS=1";
    auto m = venue_b::parse(line, 0, 0);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->price_ticks, -1'250000LL);
}

TEST(NormalizeB, FractionalPaddingAndTruncation) {
    auto m1 = venue_b::parse("V|VENUE_B|SYM=A|SIDE=B|PX=1.1|QTY=1|TS=1", 0, 0);
    ASSERT_TRUE(m1.has_value());
    EXPECT_EQ(m1->price_ticks, 1'100000LL);
    auto m2 = venue_b::parse("V|VENUE_B|SYM=A|SIDE=B|PX=1.123456789|QTY=1|TS=1", 0, 0);
    ASSERT_TRUE(m2.has_value());
    EXPECT_EQ(m2->price_ticks, 1'123456LL);
}

TEST(NormalizeB, EmptySymbolRejected) {
    std::string_view line = "V|VENUE_B|SYM=|SIDE=B|PX=1|QTY=1|TS=1";
    EXPECT_FALSE(venue_b::parse(line, 0, 0).has_value());
}

TEST(NormalizeB, OverflowQtyRejected) {
    std::string_view line = "V|VENUE_B|SYM=A|SIDE=B|PX=1|QTY=99999999999|TS=1";
    EXPECT_FALSE(venue_b::parse(line, 0, 0).has_value());
}

TEST(NormalizeB, HashIsDeterministic) {
    EXPECT_EQ(venue_b::hash_symbol("AAPL"), venue_b::hash_symbol("AAPL"));
    EXPECT_NE(venue_b::hash_symbol("AAPL"), venue_b::hash_symbol("MSFT"));
}

TEST(NormalizeB, SerializeRoundTrip) {
    MdMessage in{};
    in.type = MsgType::Quote;
    in.venue_id = kVenueB;
    in.side = Side::Ask;
    in.symbol_id = venue_b::hash_symbol("TSLA");
    in.price_ticks = 250'750000LL;
    in.qty = 7;
    in.ts_ns_venue = 1715111222333444555ull;
    std::array<uint8_t, 256> buf{};
    auto n = venue_b::serialize(in, "TSLA", buf.data(), buf.size());
    ASSERT_GT(n, 0u);
    auto out = venue_b::parse(std::string_view(reinterpret_cast<const char*>(buf.data()), n), 0, 0);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(out->venue_id, kVenueB);
    EXPECT_EQ(out->symbol_id, in.symbol_id);
    EXPECT_EQ(out->side, in.side);
    EXPECT_EQ(out->price_ticks, in.price_ticks);
    EXPECT_EQ(out->qty, in.qty);
    EXPECT_EQ(out->ts_ns_venue, in.ts_ns_venue);
}

TEST(NormalizeB, GarbageNeverCrashes) {
    // Quick smoke: many short random-ish strings; we just check no crash
    // and no false-positive parse. (The fuzzer covers this exhaustively.)
    const char* inputs[] = {
        "",
        "V",
        "V|",
        "V|VENUE_B",
        "V|VENUE_B|",
        "V|VENUE_B|SYM=A",
        "\n\n\n",
        "V|VENUE_B|SYM=A|SIDE=B|PX=|QTY=1|TS=1",
        "V|VENUE_B|||||",
    };
    for (const char* p : inputs) {
        auto m = venue_b::parse(p, 0, 0);
        EXPECT_FALSE(m.has_value()) << "input='" << p << "'";
    }
}
