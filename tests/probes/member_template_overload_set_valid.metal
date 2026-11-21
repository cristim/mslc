// EXPECT: valid
// Refs #341: member templates of one name are an overload set chosen by the arguments.
#include <metal_stdlib>
using namespace metal;
struct S {
	float a;
	template <typename U> float f(U x) const { return a + float(x); }
	template <typename U> float f(U x, U y) const { return a * 3.0f + float(x) + float(y); }
	template <typename U> float f(U x, int y) const { return a * 5.0f + float(x) + float(y); }
};
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s{1.5f}; out[i] = s.f(2.0f) + s.f(1.0f, 2.0f) + s.f(1.0f, 4); }
