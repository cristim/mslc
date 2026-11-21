// EXPECT: valid
// Refs #341: slice 5d: an instance first named by a function template's body, with the template's own T, is made where its class template and arguments are first declared.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; T get() const { return v; } };
template <typename T> float f(T x) { Box<T> b; b.v = x; return float(b.get()) * 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	out[i] = f(1.5f) + f(3) + float(i);
}
