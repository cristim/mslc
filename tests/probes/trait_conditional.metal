// EXPECT: valid
// conditional<B, T, F>::type and conditional_t<B, T, F> are T where the constant B holds and F where it does not
// ([meta.trans.other]), in a declaration and as a trait's operand.
#include <metal_stdlib>
using namespace metal;
static_assert(is_same_v<conditional_t<true, int, float>, int>, "");
static_assert(is_same_v<conditional_t<(2 > 1), int, float>, int>, "");
static_assert(is_same_v<conditional<false, int, float2>::type, float2>, "");
static_assert(is_same_v<metal::conditional_t<is_scalar_v<uint2>, uint, uint2>, uint2>, "");
static_assert(!is_same_v<conditional_t<false, int, float>, int>, "");
kernel void trait_conditional(device uint* out [[buffer(0)]]) {
    conditional_t<true, uint, float> a = 7u;
    out[0] = a;
}
