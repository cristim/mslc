// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #386: an element of an array of a plain struct is not dependent; Apple takes it bare.
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
struct M { float m; template <typename T> float h(T x) const { B arr[2] = {B{m}, B{m + 1.0f}}; return arr[1].f<float>(x); } };
template <typename T> float g(T x) { B loc[2] = {B{3.0f}, B{4.0f}}; return loc[1].f<float>(x); }
kernel void member_template_element_receiver_bare_valid(device float* out [[buffer(0)]], device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]]) { M mm{in[i + 2]}; out[i] = mm.h(in[i + 1]) + g(in[i + 1]); }
