// RUN: grid=64 local=8 cases=1
// One texture sampled through a constexpr sampler with linear magnification and minification
// at fractional coordinates, so the filter weights and the texel-centre convention both show in the result.
// Coordinates are multiples of 1/1024 on a 4-texel texture, so every filter weight is a multiple of 1/256:
// Apple blends with 8-bit fixed-point weights, and any other weight differs from the exact one by up to 1/256.
// cases=1 is the moderate profile only: finite texels. The linear blend rounds differently on Apple and lavapipe,
// measured on the Apple GPU at seed 1: outA 14 ULP. The bound is that maximum.
// The edge profile (inf and NaN texels) is excluded, see known-issues.md.
// DIFFCHECK-ULP: outA 14
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> t [[texture(0)]], device float4 *outA [[buffer(0)]],
	uint i [[thread_position_in_grid]]) {
	constexpr sampler a(address::clamp_to_edge, mag_filter::linear, min_filter::linear);
	float2 uv = float2(float(int(i % 8) * 149 + 37), float(int(i / 8) * 131 + 19)) / 1024.0;
	outA[i] = t.sample(a, uv);
}
