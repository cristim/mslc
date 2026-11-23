// RUN: grid=144 local=16 cases=2
// One texture sampled through constexpr samplers (clamp to edge, repeat, mirrored repeat) with coordinates
// from -0.875 to 1.875 at texel centres. Apple has the samplers in the shader and gets no binding for them;
// the Vulkan host builds each from mslc's reflected state, so a wrong address mode there changes a result.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> t [[texture(0)]],
	device float4 *outA [[buffer(0)]], device float4 *outB [[buffer(1)]], device float4 *outC [[buffer(2)]],
	uint i [[thread_position_in_grid]]) {
	constexpr sampler a(address::clamp_to_edge, filter::nearest);
	constexpr sampler b(address::repeat, filter::nearest);
	constexpr sampler c(address::mirrored_repeat, filter::nearest);
	float2 uv = (float2(int(i % 12) - 4, int(i / 12 % 12) - 4) + 0.5) / 4.0;
	outA[i] = t.sample(a, uv);
	outB[i] = t.sample(b, uv);
	outC[i] = t.sample(c, uv);
}
