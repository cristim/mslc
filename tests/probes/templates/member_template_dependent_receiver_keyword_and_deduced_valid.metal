// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #386: a dependent receiver takes the keyword, with explicit arguments or with the arguments deduced
// ("in.template f(x)", which Apple accepts and mslc once refused with a message about no member template).
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> float g(T in) { return in.template f(1.0f) + in.template f<float>(2.0f) * 10.0f; }
kernel void member_template_dependent_receiver_keyword_and_deduced_valid(device float* out [[buffer(0)]],
	device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
	B y{in[i]};
	out[i] = g(y) + y.template f(in[i + 1]) * 100.0f;
}
