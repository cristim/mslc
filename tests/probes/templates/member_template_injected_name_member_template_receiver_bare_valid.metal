// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #386: the receiver W is the current instantiation and g is its own member template, so Apple takes the
// bare call from inside another member template, for the injected name and for a W<float> local alike.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct W {
	T v;
	template <typename U> float g(U x) const { return float(v) + float(x); }
	template <typename U> float h(U x) const { W w = *this; W<float> c{1.0f}; return w.g<float>(x) + c.g<float>(x); }
};
kernel void member_template_injected_name_member_template_receiver_bare_valid(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { W<float> w{in[i]}; out[i] = w.h(in[i + 1]); }
