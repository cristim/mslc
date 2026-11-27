// EXPECT: valid
// RUN: grid=1 local=1
// A transformation trait names a type in a for-init declaration and as an explicit template argument too. On an
// RTX 3060 out is 3 4294967294.
#include <metal_stdlib>
using namespace metal;
template <typename T> T id(T v) { return v; }
kernel void trait_type_in_for_and_template_argument(device uint* out [[buffer(0)]]) {
    uint n = 0;
    for (make_unsigned_t<int> i = 0; i < 3u; ++i) { n += 1; }
    out[0] = n;
    out[1] = id<make_unsigned_t<int>>(-2);
}
