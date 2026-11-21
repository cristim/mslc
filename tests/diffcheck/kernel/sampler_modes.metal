// RUN: grid=144 local=16 cases=2
// One texture sampled through three samplers at indices 0, 1 and 2 (clamp to edge, repeat and
// mirrored repeat in the harness) with coordinates from -0.875 to 1.875 at texel centres, outside
// the unit square. Each output is a different texel pattern, so swapping two sampler bindings,
// or the harness giving the wrong address mode to an index, changes a result.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> t [[texture(0)]], sampler a [[sampler(0)]], sampler b [[sampler(1)]], sampler c [[sampler(2)]],
	device float4 *outA [[buffer(0)]], device float4 *outB [[buffer(1)]], device float4 *outC [[buffer(2)]],
	uint i [[thread_position_in_grid]]) {
	float2 uv = (float2(int(i % 12) - 4, int(i / 12 % 12) - 4) + 0.5) / 4.0;
	outA[i] = t.sample(a, uv);
	outB[i] = t.sample(b, uv);
	outC[i] = t.sample(c, uv);
}
