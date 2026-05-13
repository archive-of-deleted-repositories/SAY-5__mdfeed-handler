#include <cstddef>
#include <cstdint>

#include "core/normalize_a.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
    auto m = mdfeed::venue_a::parse(data, size, 0, 0);
    (void)m;
    return 0;
}
