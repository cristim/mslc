// EXPECT: error no matching function for call to "f"; candidates: ; templates: template
// Refs #341: slice 5e: Tag<T> given Tag<const float> deduces T = const float and Tag<const T> deduces T = float; the two disagree, so f is no candidate. Apple rejects it.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Tag { float v; };
template <typename T> float f(Tag<T> a, Tag<const T> b) { return a.v + b.v; }
kernel void k(device float* out [[buffer(0)]])
{
	Tag<const float> a; a.v = 1.0f;
	Tag<const float> b; b.v = 2.0f;
	out[0] = f(a, b);
}
