#include "core/bbo_book.h"

namespace mdfeed {

BboBook::BboBook() = default;

BboBook::BboBook(std::size_t bucket_hint) {
    book_.reserve(bucket_hint);
}

BboBook::UpdateResult BboBook::apply(const MdMessage& msg) {
    UpdateResult r{};
    auto& s = book_[msg.symbol_id];
    s.last_recv_ns = msg.ts_ns_recv;
    s.last_seq = msg.seq;

    const bool is_bid = (msg.side == Side::Bid);
    const Price p = msg.price_ticks;
    const Qty q = msg.qty;

    if (is_bid) {
        if (q == 0) {
            // Cancel. Erodes the side only if it matches current top.
            if (s.has_bid && s.bid_price == p) {
                s.has_bid = false;
                s.bid_price = 0;
                s.bid_qty = 0;
                r.top_changed = true;
                r.eroded = true;
                r.depth_fallback = true;
            }
            return r;
        }
        if (!s.has_bid || p > s.bid_price) {
            s.bid_price = p;
            s.bid_qty = q;
            s.has_bid = true;
            r.top_changed = true;
        } else if (p == s.bid_price) {
            // Qty refresh at same level; top quote unchanged, qty updated.
            s.bid_qty = q;
        }
        // p < s.bid_price: ignored for BBO.
    } else {
        if (q == 0) {
            if (s.has_ask && s.ask_price == p) {
                s.has_ask = false;
                s.ask_price = 0;
                s.ask_qty = 0;
                r.top_changed = true;
                r.eroded = true;
                r.depth_fallback = true;
            }
            return r;
        }
        if (!s.has_ask || p < s.ask_price) {
            s.ask_price = p;
            s.ask_qty = q;
            s.has_ask = true;
            r.top_changed = true;
        } else if (p == s.ask_price) {
            s.ask_qty = q;
        }
        // p > s.ask_price: ignored for BBO.
    }
    return r;
}

std::optional<BboState> BboBook::get(SymbolId sym) const {
    auto it = book_.find(sym);
    if (it == book_.end()) return std::nullopt;
    return it->second;
}

}  // namespace mdfeed
