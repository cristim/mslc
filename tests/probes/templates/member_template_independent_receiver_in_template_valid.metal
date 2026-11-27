// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// Refs #386: inside a function template and a member template, "x.f<float>(a)" needs no keyword when x does not
// depend on a template parameter (a local or parameter of a concrete struct, a class template instance over
// concrete arguments, "this"). Apple takes all of these bare and with the keyword.
#include <metal_stdlib>
using namespace metal;
struct B { float b; template <typename U> float f(U x) const { return b + float(x); } };
template <typename T> struct Box { T v; template <typename U> float f(U x) const { return float(v) + float(x); } };
template <typename T> float g(T in, B p)
{
	B other{2.0f};
	Box<float> bx{3.0f};
	return other.f<float>(in) + p.f<float>(in) + bx.f<float>(in) + other.template f<float>(in) * 10.0f;
}
struct M {
	float m;
	template <typename U> float f(U x) const { return m + float(x); }
	template <typename T> float h(T x) const
	{
		B other{5.0f};
		Box<float> bx{7.0f};
		return this->f<float>(x) + other.f<float>(x) + bx.f<float>(x);
	}
};
kernel void member_template_independent_receiver_in_template_valid(device float* out [[buffer(0)]],
	device const float* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
	B y{in[i]};
	M mm{in[i + 1]};
	out[i] = g(in[i + 2], y) + mm.h(in[i]) * 100.0f;
}
