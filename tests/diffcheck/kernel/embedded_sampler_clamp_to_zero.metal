// RUN: grid=64 local=8 cases=1
// A constexpr clamp_to_zero sampler read outside the unit square, nearest and linear, so the host's border
// colour is compared with Apple's. Coordinates are multiples of 1/1024 on a 4-texel texture, so every linear weight is a
// multiple of 1/256 and Apple's 8-bit fixed-point weights are exact.
// cases=1 is the moderate profile only: finite texels. The linear blend rounds differently on Apple and lavapipe,
// measured on the Apple GPU at seed 1: outB 13 ULP, outA (nearest) bit-exact. The bound is that
// maximum, for the linear output only.
// The edge profile (inf and NaN texels) is excluded, see known-issues.md.
// DIFFCHECK-ULP: outB 13
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> t [[texture(0)]],
	device float4 *outA [[buffer(0)]], device float4 *outB [[buffer(1)]],
	uint i [[thread_position_in_grid]]) {
	constexpr sampler a(address::clamp_to_zero, filter::nearest);
	constexpr sampler b(address::clamp_to_zero, filter::linear);
	float2 uv = float2(float(int(i % 8) * 229 - 330), float(int(i / 8) * 229 - 330)) / 1024.0;
	outA[i] = t.sample(a, uv);
	outB[i] = t.sample(b, uv);
}
