// EXPECT: valid
// RUN: grid=4 local=4
// Refs #341: slice 5e: Tag<Tag<T>> deduces T through two instances; the result type decides the truncation.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Tag { float v; };
template <typename T> T twice(Tag<Tag<T>> w) { return T(w.v) * T(2); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Tag<Tag<int>> a; a.v = 2.7f + float(i);
	Tag<Tag<float>> b; b.v = 2.7f + float(i);
	out[2 * i] = float(twice(a));
	out[2 * i + 1] = twice(b);
}
