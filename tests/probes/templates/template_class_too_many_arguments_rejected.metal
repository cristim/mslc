// EXPECT: error takes 1 arguments, and 2 are given
// Refs #341
#include <metal_stdlib>
using namespace metal;
template <typename T> struct P { T v; };
kernel void k(device float* out [[buffer(0)]]) { P<float, int> p; p.v = 1; out[0] = p.v; }
