// EXPECT: error is declared after the class template that uses it
// Refs #341: a default argument naming a constant declared below the template.
#include <metal_stdlib>
using namespace metal;
template <typename T, int M = K> struct P { T v[M]; };
constant int K = 2;
kernel void k(device float* out [[buffer(0)]]) { P<float> p; p.v[0] = 1; out[0] = p.v[0]; }
