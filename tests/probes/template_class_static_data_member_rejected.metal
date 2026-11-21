// EXPECT: error a static data member is not supported
// Refs #341
#include <metal_stdlib>
using namespace metal;
template <typename T> struct P { static constexpr constant int k = 3; T v; };
kernel void k(device float* out [[buffer(0)]]) { P<float> p; p.v = 1; out[0] = p.v + float(P<float>::k); }
