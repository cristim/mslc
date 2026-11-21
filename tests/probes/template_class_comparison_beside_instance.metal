// EXPECT: valid
// Refs #341: "a < Box2" is a comparison; "Box<int>(a)" in the same expression is an instance.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; Box(T x) : v(x) {} };
constant int Box2 = 3;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	int a = 1;
	out[i] = (a < Box2 && Box<int>(a).v > 0) ? 1.0f : 2.0f;
}
