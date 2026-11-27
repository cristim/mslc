// EXPECT: error explicit template arguments on "f" inside a template need the keyword
// Refs #386: a field of a W w = *this local has the type T, which depends on the class parameter, so Apple asks
// for "template" on w.v.f<float>(x) when the class is instantiated as W<B>. Only the call on the local itself is bare.
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> struct W {
	T v;
	float g(float x) const { W w = *this; return w.v.f<float>(x); }
};
kernel void member_template_injected_name_dependent_field_bare_rejected(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { W<B> w{B{in[i]}}; out[i] = w.g(in[i]); }
