// EXPECT: valid
// RUN: grid=4 local=4
// Refs #341: slice 5e: a member template of a plain struct and of a class template takes Box<U>.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
struct S { template <typename U> U f(Box<U> b) const { return b.v / U(2); } };
template <typename T> struct H { T k; template <typename U> U g(Box<U> b) const { return b.v + U(k); } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	S s; H<int> h; h.k = 2;
	Box<float> a; a.v = 1.5f + float(i);
	Box<int> c; c.v = 7 + int(i);
	out[3 * i] = s.f(a);
	out[3 * i + 1] = float(s.f(c));
	out[3 * i + 2] = h.g(a);
}
