// EXPECT: valid
// RUN: grid=4 local=4
// Refs #341: slice 5e: T named by two parameters, by an instance and a plain parameter, and by a two-argument class.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename A, typename B> struct Pair { A a; B b; };
template <typename T> T add(Box<T> x, Box<T> y) { return x.v + y.v; }
template <typename T> T scaled(Box<T> x, T k) { return x.v * k; }
template <typename A, typename B> B second(Pair<A, B> p) { return p.b; }
template <typename T> T first_of_int(Pair<T, int> p) { return p.a; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Box<float> a; a.v = 1.5f + float(i);
	Box<float> b; b.v = 2.0f;
	Pair<float, int> p; p.a = 1.5f + float(i); p.b = 7 + int(i);
	out[4 * i] = add(a, b);
	out[4 * i + 1] = scaled(a, 2.0f);
	out[4 * i + 2] = float(second(p) / 2);
	out[4 * i + 3] = first_of_int(p);
}
