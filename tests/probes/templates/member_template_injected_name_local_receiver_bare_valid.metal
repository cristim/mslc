// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #386: a local of the injected class name copies the current instance and names no parameter; Apple takes
// the bare "w.f<float>(x)" because f is a member of the current instantiation.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct W {
	T v;
	template <typename U> float f(U x) const { return float(v) + float(x); }
	float g(float x) const { W w = *this; return w.f<float>(x); }
};
kernel void member_template_injected_name_local_receiver_bare_valid(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { W<float> w{in[i]}; out[i] = w.g(in[i + 1]); }
