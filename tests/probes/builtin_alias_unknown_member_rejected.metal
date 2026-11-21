// EXPECT: error function "metal::ghost" is not a builtin mslc recognises
// M::ghost is not in the library, so it is not a builtin function (Apple: no member named ghost).
#include <metal_stdlib>
namespace M = ::metal;
struct S { float x; };
kernel void kern(device float* out [[buffer(0)]]) { out[0] = M::ghost(1.0f); }
