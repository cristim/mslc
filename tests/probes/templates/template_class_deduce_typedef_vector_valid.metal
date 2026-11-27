// EXPECT: valid
// RUN: grid=4 local=4
// Refs #341: slice 5e: a typedef of the instance, a vector argument, a call from a helper and from a function template body.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
typedef Box<float> BF;
template <typename T> T unbox(Box<T> b) { return b.v; }
float helper(Box<float> b) { return unbox(b) + 1.0f; }
template <typename U> U wrap(U x) { Box<U> b; b.v = x; return unbox(b); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	BF a; a.v = 1.5f + float(i);
	Box<float2> v; v.v = float2(1.0f, 2.0f + float(i));
	float2 r = unbox(v);
	out[4 * i] = unbox(a);
	out[4 * i + 1] = r.x + r.y;
	out[4 * i + 2] = helper(a);
	out[4 * i + 3] = wrap(2.5f + float(i));
}
