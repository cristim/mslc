// EXPECT: error explicit template arguments on "f" inside a template need the keyword
// Refs #386: a field of an element of an injected-name array local has the type T, so Apple asks for "template" on
// ws[1].v.f<float>(x) when the class is instantiated as W<B>. The call on the element itself stays bare.
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> struct W {
	T v;
	template <typename U> float f(U x) const { return float(x); }
	float g(float x) const { W ws[2] = {*this, *this}; return ws[1].v.f<float>(x); }
};
kernel void member_template_injected_name_element_field_bare_rejected(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { W<B> w{B{in[i]}}; out[i] = w.g(in[i]); }
