// EXPECT: valid
// DISASM-MATCH: OpFunctionCall %float
// Refs #341: an instance is a type argument of another, and a member makes another instance of its own parameter.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; Box(T x) : v(x) {} T get() const { return v; } };
template <typename T> struct Pair { T a; T b; float sum() const { Box<T> x(a); Box<T> y(b); return float(x.get() + y.get()); } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Pair<float> p;
	p.a = 1.0f; p.b = 2.0f;
	out[i] = p.sum();
}
