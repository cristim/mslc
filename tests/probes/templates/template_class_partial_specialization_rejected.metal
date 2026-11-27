// EXPECT: error a partial or explicit specialization of the class template "P" is not lowered
// Refs #341
#include <metal_stdlib>
using namespace metal;
template <typename T> struct P { T v; };
template <typename T> struct P<thread T*> { int v; };
kernel void k(device float* out [[buffer(0)]]) { P<float> p; p.v = 1; out[0] = p.v; }
