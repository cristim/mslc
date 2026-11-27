// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// vec<T, N> as a class template's argument, for a local and a helper's parameter.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
static float h(Box<vec<float, 2> > b) { return b.v.y; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { Box<vec<float, 2> > b; b.v = float2(1, float(i)); out[i] = b.v.x + h(b); }
