// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #386: the field of a plain struct is not dependent inside its member template; Apple takes it bare.
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
struct M { B inner; float m; template <typename T> float h(T x) const { return inner.f<float>(x); } };
kernel void member_template_field_receiver_bare_valid(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { M mm{B{in[i]}, 1.0f}; out[i] = mm.h(in[i + 1]); }
