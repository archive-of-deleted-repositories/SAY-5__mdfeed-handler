#include "core/bbo_book.h"

#include <gtest/gtest.h>

#include "core/types.h"

using namespace mdfeed;

namespace {

MdMessage q(SymbolId sym, Side side, Price px, Qty qty, uint64_t seq = 1) {
    MdMessage m{};
    m.type = MsgType::Quote;
    m.venue_id = kVenueA;
    m.symbol_id = sym;
    m.side = side;
    m.price_ticks = px;
    m.qty = qty;
    m.ts_ns_recv = 100;
    m.seq = seq;
    return m;
}

}  // namespace

TEST(BboBook, EmptyBookLookupReturnsNullopt) {
    BboBook b;
    EXPECT_FALSE(b.get(1).has_value());
    EXPECT_EQ(b.size(), 0u);
}

TEST(BboBook, FirstBidSetsTop) {
    BboBook b;
    auto r = b.apply(q(1, Side::Bid, 100, 10));
    EXPECT_TRUE(r.top_changed);
    EXPECT_FALSE(r.eroded);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_TRUE(s->has_bid);
    EXPECT_FALSE(s->has_ask);
    EXPECT_EQ(s->bid_price, 100);
    EXPECT_EQ(s->bid_qty, 10u);
}

TEST(BboBook, FirstAskSetsTop) {
    BboBook b;
    b.apply(q(1, Side::Ask, 110, 5));
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_TRUE(s->has_ask);
    EXPECT_EQ(s->ask_price, 110);
    EXPECT_EQ(s->ask_qty, 5u);
}

TEST(BboBook, BetterBidReplacesTop) {
    BboBook b;
    b.apply(q(1, Side::Bid, 100, 10));
    auto r = b.apply(q(1, Side::Bid, 101, 7));
    EXPECT_TRUE(r.top_changed);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->bid_price, 101);
    EXPECT_EQ(s->bid_qty, 7u);
}

TEST(BboBook, WorseBidIgnoredForTop) {
    BboBook b;
    b.apply(q(1, Side::Bid, 100, 10));
    auto r = b.apply(q(1, Side::Bid, 99, 50));
    EXPECT_FALSE(r.top_changed);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->bid_price, 100);
    EXPECT_EQ(s->bid_qty, 10u);
}

TEST(BboBook, BetterAskReplacesTop) {
    BboBook b;
    b.apply(q(1, Side::Ask, 110, 5));
    auto r = b.apply(q(1, Side::Ask, 109, 8));
    EXPECT_TRUE(r.top_changed);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->ask_price, 109);
    EXPECT_EQ(s->ask_qty, 8u);
}

TEST(BboBook, WorseAskIgnoredForTop) {
    BboBook b;
    b.apply(q(1, Side::Ask, 110, 5));
    auto r = b.apply(q(1, Side::Ask, 111, 50));
    EXPECT_FALSE(r.top_changed);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->ask_price, 110);
}

TEST(BboBook, SamePriceBidIsQtyRefresh) {
    BboBook b;
    b.apply(q(1, Side::Bid, 100, 10));
    auto r = b.apply(q(1, Side::Bid, 100, 25));
    EXPECT_FALSE(r.top_changed);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->bid_price, 100);
    EXPECT_EQ(s->bid_qty, 25u);
}

TEST(BboBook, SamePriceAskIsQtyRefresh) {
    BboBook b;
    b.apply(q(1, Side::Ask, 110, 5));
    auto r = b.apply(q(1, Side::Ask, 110, 12));
    EXPECT_FALSE(r.top_changed);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->ask_qty, 12u);
}

TEST(BboBook, CancelMatchingBidErodes) {
    BboBook b;
    b.apply(q(1, Side::Bid, 100, 10));
    auto r = b.apply(q(1, Side::Bid, 100, 0));
    EXPECT_TRUE(r.top_changed);
    EXPECT_TRUE(r.eroded);
    EXPECT_TRUE(r.depth_fallback);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_FALSE(s->has_bid);
    EXPECT_EQ(s->bid_price, 0);
    EXPECT_EQ(s->bid_qty, 0u);
}

TEST(BboBook, CancelMatchingAskErodes) {
    BboBook b;
    b.apply(q(1, Side::Ask, 110, 5));
    auto r = b.apply(q(1, Side::Ask, 110, 0));
    EXPECT_TRUE(r.top_changed);
    EXPECT_TRUE(r.eroded);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_FALSE(s->has_ask);
}

TEST(BboBook, CancelDifferentPriceDoesNothing) {
    BboBook b;
    b.apply(q(1, Side::Bid, 100, 10));
    auto r = b.apply(q(1, Side::Bid, 99, 0));
    EXPECT_FALSE(r.top_changed);
    EXPECT_FALSE(r.eroded);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->bid_price, 100);
}

TEST(BboBook, CrossingBidLiftsAskSideUnchanged) {
    // Per spec: BBO records what venue sends. A bid at 111 with ask at 110
    // currently doesn't auto-uncross, venue is expected to send the ask
    // cancellation. We do record the new (crossed) bid as the new top.
    BboBook b;
    b.apply(q(1, Side::Ask, 110, 5));
    auto r = b.apply(q(1, Side::Bid, 111, 7));
    EXPECT_TRUE(r.top_changed);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->bid_price, 111);
    EXPECT_EQ(s->ask_price, 110);
    EXPECT_TRUE(s->has_bid);
    EXPECT_TRUE(s->has_ask);
}

TEST(BboBook, MultiSymbolIndependence) {
    BboBook b;
    b.apply(q(1, Side::Bid, 100, 10));
    b.apply(q(2, Side::Bid, 200, 20));
    b.apply(q(3, Side::Bid, 300, 30));
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(b.get(1)->bid_price, 100);
    EXPECT_EQ(b.get(2)->bid_price, 200);
    EXPECT_EQ(b.get(3)->bid_price, 300);
}

TEST(BboBook, EachSymbolHasIndependentBidAndAsk) {
    BboBook b;
    b.apply(q(1, Side::Bid, 100, 10));
    b.apply(q(1, Side::Ask, 105, 8));
    b.apply(q(2, Side::Ask, 205, 18));
    auto s1 = b.get(1);
    ASSERT_TRUE(s1.has_value());
    EXPECT_TRUE(s1->has_bid);
    EXPECT_TRUE(s1->has_ask);
    auto s2 = b.get(2);
    ASSERT_TRUE(s2.has_value());
    EXPECT_FALSE(s2->has_bid);
    EXPECT_TRUE(s2->has_ask);
}

TEST(BboBook, RecordsLastRecvAndSeq) {
    BboBook b;
    MdMessage m = q(1, Side::Bid, 100, 10, 42);
    m.ts_ns_recv = 999'999;
    b.apply(m);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->last_recv_ns, 999'999u);
    EXPECT_EQ(s->last_seq, 42u);
}

TEST(BboBook, ZeroQtyOnNewSideIgnored) {
    // No top yet, cancel arrives. Should not create state on that side.
    BboBook b;
    auto r = b.apply(q(1, Side::Bid, 100, 0));
    EXPECT_FALSE(r.top_changed);
    EXPECT_FALSE(r.eroded);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_FALSE(s->has_bid);
}

TEST(BboBook, RepeatedBetterBidsClimbMonotonically) {
    BboBook b;
    for (int i = 1; i <= 20; ++i) {
        b.apply(q(1, Side::Bid, 100 + i, 1));
    }
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->bid_price, 120);
}

TEST(BboBook, BucketHintConstructorCompiles) {
    BboBook b(1024);
    EXPECT_EQ(b.size(), 0u);
    b.apply(q(1, Side::Bid, 100, 10));
    EXPECT_EQ(b.size(), 1u);
}

TEST(BboBook, ErosionFollowedByNewBidRecovers) {
    BboBook b;
    b.apply(q(1, Side::Bid, 100, 10));
    b.apply(q(1, Side::Bid, 100, 0));
    auto r = b.apply(q(1, Side::Bid, 99, 20));
    EXPECT_TRUE(r.top_changed);
    auto s = b.get(1);
    ASSERT_TRUE(s.has_value());
    EXPECT_TRUE(s->has_bid);
    EXPECT_EQ(s->bid_price, 99);
    EXPECT_EQ(s->bid_qty, 20u);
}
