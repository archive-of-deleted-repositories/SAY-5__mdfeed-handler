#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>

#include "core/types.h"

namespace mdfeed {

// Per-symbol BBO book. Top-of-book only; full depth is intentionally out
// of scope (see docs/bbo-vs-depth.md and SAY-5/orderbook-sim).
class BboBook {
public:
    BboBook();
    explicit BboBook(std::size_t bucket_hint);

    // Update result tells the caller whether the top-of-book changed and
    // whether the update was "absorbed" (matching old best, qty refresh) or
    // had to invalidate the side because the only known level eroded.
    struct UpdateResult {
        bool top_changed = false;
        bool eroded = false;          // qty=0 cancel hit current best
        bool depth_fallback = false;  // out-of-band: see notes below
    };

    // Apply a single update. Semantics:
    //  - qty > 0  : (price, qty) overrides the named side if (a) no top yet,
    //               (b) strictly better than current top, or (c) equal to
    //               current top (qty refresh).
    //  - qty == 0 : cancel at that price. If price matches current top on
    //               that side, mark the side as having no top and request
    //               depth fallback (the BBO-only book cannot recover the
    //               next-best price; see UpdateResult.depth_fallback).
    //
    // Updates with prices worse than the current best on the same side are
    // ignored for BBO purposes (top doesn't move; depth book would record).
    UpdateResult apply(const MdMessage& msg);

    // Lookup current state. Returns nullopt if symbol not seen.
    std::optional<BboState> get(SymbolId sym) const;

    // Number of symbols tracked.
    std::size_t size() const { return book_.size(); }

private:
    std::unordered_map<SymbolId, BboState> book_;
};

}  // namespace mdfeed
