// EXPECT: valid
// RUN: grid=4 local=4
// Refs #341: slice 5e: a function that takes Box<float> beats the template; the template serves Box<int>; explicit arguments fix T (also only the first of two).
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> float which(Box<T> b) { return 1.0f; }
float which(Box<float> b) { return 2.0f; }
template <typename T> T unbox(Box<T> b) { return b.v; }
template <typename A, typename B> struct Pair { A a; B b; };
template <typename A, typename B> A pick(Pair<A, B> p, B x) { return p.a + A(x); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Pair<float, int> p; p.a = 1.5f + float(i); p.b = 7;
	Box<float> a; a.v = 1.5f + float(i);
	Box<int> c; c.v = 7 + int(i);
	out[4 * i] = which(a);
	out[4 * i + 1] = which(c);
	out[4 * i + 2] = unbox<float>(a) + float(unbox<int>(c));
	out[4 * i + 3] = pick<float>(p, 3);
}
