// EXPECT: error write ".template f<...>(...)"
// Refs #386: a field of a class template may have a parameter type; Apple rejects it bare (use 'template' keyword).
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> struct W { T v; float g(B p, float x) const { return v.f<float>(x); } };
kernel void member_template_class_template_field_receiver_bare_rejected(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { W<B> w{B{1.0f}}; out[i] = w.g(B{2.0f}, in[i]); }
