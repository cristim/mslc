#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float, access::write> t [[texture(0)]], device float *o [[buffer(0)]],
	uint g [[thread_position_in_grid]]) {
	if (g < 16)
		t.write(float4(float(g) + 1.0f, float(g) * 2.0f, -float(g), 1.0f), uint2(g % 4, g / 4));
	o[g] = float(g) + 1.0f;
}
