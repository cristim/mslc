// EXPECT: error explicit template arguments on "f" inside a template need the keyword
// Refs #386: an element of a field of a W w = *this local has a type built from T, so the bare call is dependent.
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> struct W {
	T arr[2];
	float g(float x) const { W w = *this; return w.arr[0].f<float>(x); }
};
kernel void member_template_injected_name_dependent_element_bare_rejected(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { W<B> w{{B{in[i]}, B{in[i]}}}; out[i] = w.g(in[i]); }
