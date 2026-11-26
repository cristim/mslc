#include <metal_stdlib>
using namespace metal;
kernel void k(texture3d<float, access::read> t [[texture(0)]], device float4 *o [[buffer(0)]],
	uint3 g [[thread_position_in_grid]]) {
	o[g.x] = t.read(g);
}
