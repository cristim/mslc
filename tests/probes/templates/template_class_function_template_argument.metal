// EXPECT: valid
// Refs #341: a function template deduces an instance as its type argument, and explicit arguments may name one.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; Box(T x) : v(x) {} T get() const { return v; } };
template <typename U> float f(U x) { return float(x.get()) * 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Box<float> b(2.0f);
	Box<int> c(3);
	out[i] = f(b) + f(c) + f<Box<float>>(b);
}
