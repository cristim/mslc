// EXPECT: valid
// Refs #341: slice 5d: a function template that names an instance with no template parameter in it, and a caller that names the same instance afterwards.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; T get() const { return v; } };
template <typename T> float f(T x) { Box<float> b; b.v = float(x) + 0.5f; return b.get(); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Box<float> c{ 4.0f };
	out[i] = f(2) + f(1.25f) + c.get();
}
