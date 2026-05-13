# Wire Formats

Two simulated venues; both normalize to the same internal `MdMessage`.

## Venue A, fixed-width binary, 27 bytes, little-endian

| Offset | Size | Field        | Notes |
|--------|------|--------------|-------|
| 0      | 1    | `msg_type`   | Low 7 bits: 1 = Quote, 2 = Cancel. High bit: 0 = Bid, 1 = Ask |
| 1      | 2    | `venue_id`   | Must equal 1 (`kVenueA`) |
| 3      | 4    | `symbol_id`  | Caller-assigned 32-bit symbol id |
| 7      | 8    | `price_int`  | int64, ticks at `kPriceScale = 1_000_000` |
| 15     | 4    | `qty`        | uint32; `qty == 0` is a cancel at that price level |
| 19     | 8    | `ts_ns`      | uint64; venue-stamped wall time, ns |

Rejected by `venue_a::parse`:

- buffer shorter than 27 bytes
- unknown `msg_type` (low-7 bits not in `{1, 2}`)
- `venue_id` mismatch

Parsing is a few aligned `memcpy`s plus a side/type split; no dynamic
allocation.

## Venue B, pipe-delimited ASCII, newline terminated

```
V|VENUE_B|SYM=AAPL|SIDE=B|PX=192.34|QTY=100|TS=1715000000123456789\n
```

| Field    | Format                                       |
|----------|----------------------------------------------|
| Prefix   | Literal `V|VENUE_B|`                         |
| `SYM=`   | Ticker token, mapped to `symbol_id` via FNV-1a |
| `SIDE=`  | `B` or `A`                                   |
| `PX=`    | Decimal price; padded/truncated to 6 fractional digits, scaled to ticks |
| `QTY=`   | uint32; `0` is a cancel                      |
| `TS=`    | uint64 venue ns                              |

Rejected by `venue_b::parse`:

- missing prefix
- any missing field
- empty `SYM=`
- non-`B`/`A` `SIDE=`
- non-numeric `PX=`, `QTY=`, or `TS=`
- `QTY=` value > `UINT32_MAX`

The fuzz harness (`tests/fuzz/normalize_b_fuzz.cpp`) exercises arbitrary
byte sequences; the CI smoke runs each parser for 5000 iterations.

## Normalized internal form

```cpp
struct MdMessage {
    MsgType  type;
    VenueId  venue_id;
    SymbolId symbol_id;
    Side     side;
    Price    price_ticks;    // int64 ticks at kPriceScale
    Qty      qty;            // 0 means cancel
    TsNs     ts_ns_venue;    // wire timestamp
    TsNs     ts_ns_recv;     // CLOCK_MONOTONIC_RAW at recvfrom return
    uint64_t seq;            // per-venue sequence number
};
```

Both venues hash symbol tickers via FNV-1a so identical tickers map to
identical `symbol_id`s in the unified book.

## Adding a real protocol (out of scope, sketch only)

Real venues use FIX, ITCH, PITCH, etc. The swap-in path is:

1. Write `src/core/normalize_<protocol>.{h,cpp}` exposing the same
   `parse(buf, len, recv_ns, seq) -> optional<MdMessage>` signature.
2. Add a dispatch byte (or session-level routing) in
   `FeedHandler::run()` similar to the venue A/B disambiguation today.
3. Replace the simulator with a captured PCAP replayer (still over
   loopback UDP for hermetic CI; documented out of scope here).
