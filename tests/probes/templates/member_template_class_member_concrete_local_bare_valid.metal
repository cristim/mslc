// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #386: a concrete local in a member of a class template is not dependent; Apple takes it bare.
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> struct W { T v; float g(float x) const { B b{2.0f}; return b.f<float>(x) + float(v); } };
kernel void member_template_class_member_concrete_local_bare_valid(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { W<float> w{in[i]}; out[i] = w.g(in[i + 1]); }
