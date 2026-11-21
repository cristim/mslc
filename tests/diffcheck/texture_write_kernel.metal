#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float, access::write> t [[texture(0)]], device float *o [[buffer(0)]],
	uint2 g [[thread_position_in_grid]]) {
	t.write(float4(1.0), g);
	o[g.x] = 1.0;
}
