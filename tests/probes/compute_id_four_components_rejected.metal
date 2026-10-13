// EXPECT: error which has to be a uint or ushort scalar or a vector of two or three of them
// Apple rejects a uint4 compute id, and so must mslc: the builtin has three lanes.
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint* out [[buffer(0)]], uint4 g [[thread_position_in_grid]])
{
	out[0] = g.x;
}
