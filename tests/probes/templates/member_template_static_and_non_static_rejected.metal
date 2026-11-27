// EXPECT: error is declared both static and not static, which Apple takes and mslc does not lower
// Refs #341
#include <metal_stdlib>
using namespace metal;
struct S {
	float a;
	template <typename U> static U f(U x) { return x; }
	template <typename U> U f(U x, U y) const { return x + y + a; }
};
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = S::f(2.0f); }
