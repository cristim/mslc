// EXPECT: valid
// DISASM-MATCH: OpVectorShuffle %v2uint %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ 0 1
// A compute id declared as a two-component vector is the first two lanes of the
// uvec3 Vulkan fixes, for uint2 and ushort2 and for all three ids.
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint2* out [[buffer(0)]],
              uint2 g [[thread_position_in_grid]],
              ushort2 l [[thread_position_in_threadgroup]],
              ushort2 w [[threadgroup_position_in_grid]])
{
	out[0] = g;
	out[1] = uint2(l);
	out[2] = uint2(w);
}
