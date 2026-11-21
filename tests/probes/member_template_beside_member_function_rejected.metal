// EXPECT: error is both a member function and a member function template
// Refs #341: Apple takes the two as one overload set, which mslc does not rank.
#include <metal_stdlib>
using namespace metal;
struct S {
	float a;
	float f(float x) const { return a + x; }
	template <typename U> float f(U x) const { return a - float(x); }
};
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ S s{1.5f}; out[i] = s.f(2.0f) + s.f(2); }
