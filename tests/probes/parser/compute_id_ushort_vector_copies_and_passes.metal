// EXPECT: valid
// A ushort2 compute id is narrowed to the declared element type, so it
// can be copied into a local and passed to a function that takes it.
#include <metal_stdlib>
using namespace metal;
uint sum(ushort2 v) { return v.x + v.y; }
kernel void k(device uint* out [[buffer(0)]], ushort2 g [[thread_position_in_grid]],
              ushort2 w [[threadgroup_position_in_grid]])
{
	ushort2 p = g;
	ushort2 q = w;
	out[0] = sum(p) + q.y;
}
