// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=2048
// DISASM: OpFunctionCall
// Refs #341: constructor templates in a plain struct and a class template instance, with direct
// construction S s(x), functional expression S(x) and static_cast<S>(x), beside a default constructor.
#include <metal_stdlib>
using namespace metal;
struct S {
	float a;
	float b;
	S() : a(1.0f), b(2.0f) {}
	template <typename U> S(U x) : b(float(x) + 1.0f), a(float(x)) {
		if (x < U(0)) {
			return;
		}
		a += scale(b);
	}
	template <typename U, typename V> S(U x, V y) : a(float(x)), b(float(y)) {}
	float scale(float v) const { return v; }
};
typedef S SAlias;
template <typename W> W makeWrap(int x) { return W(x); }
template <typename T> struct Box {
	T v;
	Box(T x) : v(x) {}
	template <typename U> Box(U x) : v(T(x) * T(2)) {}
	template <typename V> T pick(V y) const {
		T val = T(y);
		Box tmp(val);
		return tmp.v;
	}
};
struct UsesBox {
	float x;
	UsesBox() : x(Box<float>(1.0f).v) {}
};
kernel void member_template_constructor_valid(device float* out [[buffer(0)]],
                                              device const float* in [[buffer(1)]],
                                              uint i [[thread_position_in_grid]])
{
	S d;
	S s1(2);
	S s2 = S(in[i], 3);
	S s3 = static_cast<S>(4);
	SAlias s4 = SAlias(5);
	S s5 = makeWrap<S>(-1);
	Box<float> b(in[i]);
	Box<float> bInt(3);
	UsesBox ub;
	out[i] = d.a + d.b + s1.a + s2.a + s2.b + s3.a + s4.a + s5.a + b.v + bInt.v + b.pick(4) + ub.x;
}
