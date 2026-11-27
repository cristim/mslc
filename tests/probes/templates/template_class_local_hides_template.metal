// EXPECT: valid
// Refs #341: a local named like a class template is the local: "Box < q" compares, it does not open template arguments.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; Box(T x) : v(x) {} };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	float Box = 1.0f;
	float q = 3.0f;
	out[i] = (Box < q) ? 1.0f : 2.0f;
}
