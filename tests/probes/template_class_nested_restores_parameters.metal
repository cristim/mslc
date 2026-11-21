// EXPECT: valid
// DISASM-NOT: OpConvertFToS
// Refs #341: an instance made inside another one leaves the outer parameters as they were (T is float after Box<U> was read with T bound to int).
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; Box(T x) : v(x) {} T get() const { return v; } };
template <typename T, typename U> struct Pair {
	T a; U b;
	float f() const { Box<U> x(b); return float(x.get()); }
	T h() const { T t = a; return t; }
};
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Pair<float, int> p;
	p.a = 1.5f; p.b = 2;
	out[i] = p.f() + p.h();
}
