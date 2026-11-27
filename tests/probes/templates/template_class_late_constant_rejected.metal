// EXPECT: error is declared after the class template that uses it
// Refs #341
#include <metal_stdlib>
using namespace metal;
template <typename T> struct P { T v[K]; };
constant int K = 2;
kernel void k(device float* out [[buffer(0)]]) { P<float> p; p.v[0] = 1; out[0] = p.v[0]; }
