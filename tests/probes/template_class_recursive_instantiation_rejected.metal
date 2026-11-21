// EXPECT: error nests deeper than 64 levels
// Refs #341
#include <metal_stdlib>
using namespace metal;
template <int N> struct Q { Q<N + 1>* p; int v; };
kernel void k(device float* out [[buffer(0)]]) { Q<0> q; q.v = 1; out[0] = float(q.v); }
