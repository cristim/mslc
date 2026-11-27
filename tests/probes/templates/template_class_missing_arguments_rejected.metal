// EXPECT: error too few template arguments
// Refs #341
#include <metal_stdlib>
using namespace metal;
template <typename T, int N> struct P { T v[N]; };
kernel void k(device float* out [[buffer(0)]]) { P<float> p; p.v[0] = 1; out[0] = p.v[0]; }
