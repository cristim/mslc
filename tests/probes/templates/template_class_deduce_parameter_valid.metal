// EXPECT: valid
// RUN: grid=4 local=4
// Refs #341: slice 5e: a parameter Box<T> deduces T from the instance; int and float give different results (7/2).
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> T halve(Box<T> b) { return b.v / T(2); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Box<float> a; a.v = 7.0f + float(i);
	Box<int> c; c.v = 7 + int(i);
	out[2 * i] = halve(a);
	out[2 * i + 1] = float(halve(c));
}
