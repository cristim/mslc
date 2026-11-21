// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #386: the result of a plain function called with literals is not dependent; Apple takes it bare.
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
B mk(float v) { return B{v}; }
B mk0() { return B{5.0f}; }
template <typename T> float g(T x) { return mk(2.0f).f<float>(x) + mk0().f<float>(x); }
kernel void member_template_call_result_receiver_bare_valid(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { out[i] = g(in[i]); }
