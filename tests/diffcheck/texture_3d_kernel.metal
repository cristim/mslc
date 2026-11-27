#include <metal_stdlib>
using namespace metal;
kernel void k(texture3d<float, access::read> t [[texture(0)]], device float4 *o [[buffer(0)]],
	uint g [[thread_position_in_grid]]) {
	o[g] = t.read(uint3(g % 4, g / 4 % 4, g / 16 % 2));
}
