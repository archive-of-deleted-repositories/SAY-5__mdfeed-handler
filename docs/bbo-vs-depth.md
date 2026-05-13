# BBO vs Full-Depth Book

This study tracks **top of book** per symbol: the best bid and best ask
price/quantity, nothing deeper. Full depth-by-price (multiple levels per
side, queue position, order-by-order) is a separate problem.

## Why BBO here

- The companion repo `SAY-5/orderbook-sim` already studies the full
  depth book + price-time priority matching side of the problem. Going
  back to that here would be duplication.
- BBO is the right granularity for a feed-handler-focused study: the
  question is "how fast can I get a normalized top-of-book update from a
  UDP wire into in-process state, and how do I measure that honestly?"
- Most algorithmic trading strategies that consume market data are
  driven by top-of-book changes. Full depth is needed for queue-position
  modelling and large-order workups, which are downstream of this
  problem.

## What this means in practice

The `BboBook` API has two non-obvious behaviours that follow from the
top-only invariant:

1. **Worse-side updates are ignored.** A bid at 99 when the current
   best bid is 100 doesn't move the top; in a depth book it would
   record at level 2. Here it's silently dropped.

2. **Erosion can't recover.** If a cancel hits the current best, the
   side goes empty and we wait for the next venue update. A depth book
   would promote level 2 to top instantly. `UpdateResult.depth_fallback`
   surfaces this so a consumer can decide what to do (request a snapshot,
   pause strategies for that symbol, log, etc.). In the current
   implementation we just log via counters.

## Honest scope notes

- **Crossed books are recorded as-is.** If a bid arrives at a price
  above the current ask, both sides remain set (the new bid wins on the
  bid side; the existing ask is left untouched). Real venues are
  expected to send a corresponding ask cancellation; uncrossing logic
  is not the feed handler's job.
- **No order-by-order tracking.** Each `MdMessage` is treated as a level
  replace/cancel, not as an add/modify/delete against individual orders.
  ITCH/OUCH-style order-book replay would need a separate data
  structure.
- **No symbol metadata.** Tick sizes, lot sizes, trading status, halt
  flags, etc., are all out of scope. The book is a pure quote state.

## Cross-link

For full depth + matching + price-time priority, see
[`SAY-5/orderbook-sim`](https://github.com/SAY-5/orderbook-sim).
