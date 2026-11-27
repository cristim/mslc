#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d_ms<float> t [[texture(0)]], device float4 *o [[buffer(0)]],
	uint i [[thread_position_in_grid]]) {
	o[i] = t.read(uint2(i % 4, i / 4 % 4), 0);
}
