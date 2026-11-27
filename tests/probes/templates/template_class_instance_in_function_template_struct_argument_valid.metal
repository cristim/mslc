// EXPECT: valid
// Refs #341: slice 5d: a function template instance names Box<P>, P a struct declared above the function template.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; T get() const { return v; } };
struct P { float a; int n; };
template <typename T> float f(T x) { Box<T> b; b.v = x; return b.get().a + float(b.get().n); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	P p;
	p.a = 6.0f;
	p.n = int(i);
	out[i] = f(p);
}
