// EXPECT: valid
// RUN: grid=1
// A default template argument chosen with conditional_t from the parameters before it, as ANGLE's imul helper
// writes it: the product is X for a scalar Y and Y for a vector one. On an RTX 3060 out is 6 8 4294967295 12.
#include <metal_stdlib>
using namespace metal;
template <typename X, typename Y, typename Z = metal::conditional_t<metal::is_scalar_v<Y>, X, Y>>
Z imul(X x, Y y) { return as_type<Z>(Z(x) * Z(y)); }
kernel void trait_conditional_default_template_argument(device uint* out [[buffer(0)]]) {
    out[0] = imul(2u, 3u);
    uint2 v = imul(2u, uint2(3, 4));
    out[1] = v.y;
    out[2] = uint(imul(-1, 1u) == -1 ? 4294967295u : 0u);
    uint3 w = imul(1u, uint3(5));
    out[3] = w.z + 7u;
}
