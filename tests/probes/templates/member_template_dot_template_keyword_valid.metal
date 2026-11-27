// EXPECT: valid
// DISASM: OpFAdd
// Refs #384: ".template" and "this->template" spell a member template call; Apple takes both, in a class template
// body, a function template body and outside any template.
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> struct W {
	T v;
	template <typename U> float f(U x) const { return float(v) + float(x); }
	float g(B in) const { return in.template f<float>(v); }
	float h() const { return this->template f<float>(1.0f) + this->f<float>(2.0f); }
};
template <typename T> float fn(T in) { return in.template f<float>(1.0f); }
kernel void k(device float* o [[buffer(0)]])
{
	B y{2.0f};
	W<float> w{1.0f};
	o[0] = y.template f<float>(1.0f) + w.g(y) + w.h() + fn(y);
}
