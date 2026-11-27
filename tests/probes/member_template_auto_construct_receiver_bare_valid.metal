// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #386: auto a = B{...} has the type B; Apple takes it bare.
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> float g(T x) { auto a = B{1.0f}; return a.f<float>(x); }
kernel void member_template_auto_construct_receiver_bare_valid(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { out[i] = g(in[i]); }
