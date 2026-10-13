// EXPECT: error which has to be a uint or ushort scalar or a vector of two or three of them
// Apple rejects an int compute id.
#include <metal_stdlib>
using namespace metal;
kernel void k(device int* out [[buffer(0)]], int2 g [[thread_position_in_grid]])
{
	out[0] = g.x;
}
