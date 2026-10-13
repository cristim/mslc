// EXPECT: valid
// A uint2 thread_position_in_grid is a uint2 coordinate for texture read.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float, access::read> t [[texture(0)]], device float4* out [[buffer(0)]],
              uint2 g [[thread_position_in_grid]])
{
	out[0] = t.read(g);
}
