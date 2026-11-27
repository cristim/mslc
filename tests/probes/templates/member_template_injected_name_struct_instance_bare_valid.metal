// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #386: Apple accepts the bare w.f<float>(x) on a W w = *this local when the class instance is W<B> with a
// struct argument and the member takes a B p parameter: f is a member of the current instantiation.
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> struct W {
	T v;
	template <typename U> float f(U x) const { return float(x); }
	float g(B p, float x) const { W w = *this; return w.f<float>(x); }
};
kernel void member_template_injected_name_struct_instance_bare_valid(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { W<B> w{B{in[i]}}; out[i] = w.g(B{in[i]}, in[i]); }
