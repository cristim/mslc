// EXPECT: valid
// RUN: grid=4 local=4
// Refs #341: slice 5e: Box<T> as a return type, with T deduced from another parameter, and a call result as an argument.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename A, typename B> struct Pair { A a; B b; };
template <typename T> T unbox(Box<T> b) { return b.v; }
template <typename T> Box<T> mk(T x) { Box<T> b; b.v = x; return b; }
template <typename A, typename B> Pair<B, A> swp(Pair<A, B> p) { Pair<B, A> r; r.a = p.b; r.b = p.a; return r; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Box<float> b = mk(1.5f + float(i));
	Box<int> c = mk(3 + int(i));
	Pair<float, int> p; p.a = 1.5f + float(i); p.b = 7 + int(i);
	Pair<int, float> r = swp(p);
	out[5 * i] = b.v;
	out[5 * i + 1] = float(c.v);
	out[5 * i + 2] = unbox(mk(2.0f + float(i)));
	out[5 * i + 3] = float(r.a);
	out[5 * i + 4] = r.b;
}
