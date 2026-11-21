// RUN: grid=64 local=16 cases=3
// Two textures at indices that are not in parameter order, one sampler at index 1, and buffers
// at indices 0 and 1 and 2 (a constant scale, two outputs). Sampled at texel centres with a
// nearest sampler, so the result is one texel exactly. The second texture is sampled with the
// coordinates swapped, so the texture order and the coordinate order both matter.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> a [[texture(2)]], device float4 *outA [[buffer(1)]], texture2d<float> b [[texture(0)]],
	sampler s [[sampler(1)]], constant float &scale [[buffer(0)]], device float4 *outB [[buffer(2)]],
	uint i [[thread_position_in_grid]]) {
	float2 uv = (float2(i % 4, i / 4 % 4) + 0.5) / 4.0;
	outA[i] = a.sample(s, uv) * scale;
	outB[i] = b.sample(s, uv.yx);
}
