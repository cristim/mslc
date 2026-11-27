// EXPECT: valid
// DISASM-MATCH: OpFunctionCall %float %[_0-9a-zA-Z]+ %_?[0-9a-zA-Z_]+ %float_2
// Refs #341: a member function template is read per call with its arguments, the object being its first parameter.
#include <metal_stdlib>
using namespace metal;
struct S {
	float a;
	template <typename U> float f(U x) const { return a + float(x / U(2)); }
	template <typename R = float, typename U> R g(U x) const { return R(x) + R(a); }
	template <typename U> float h(thread const U& x) const { return a * float(x); }
};
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	S s{1.5f};
	int n = 3;
	out[i] = s.f(2.0f) + s.f(5) + s.f<int>(4.5f) + float(s.g<int>(2.5f)) + s.g(1.0f) + s.h(n);
}
