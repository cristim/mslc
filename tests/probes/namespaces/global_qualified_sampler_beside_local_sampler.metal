// EXPECT: valid
// RUN: grid=4 local=4 cases=3
// REFLECT: "s_address": "Repeat"
// REFLECT-NOT: "ClampToEdge"
#include <metal_stdlib>
using namespace metal;
constexpr sampler s(filter::nearest, address::repeat);
kernel void kern(texture2d<float> t [[texture(0)]], device float4* out [[buffer(0)]],
	uint gid [[thread_position_in_grid]]) {
	constexpr sampler s(filter::nearest, address::clamp_to_edge);
	float2 uv = float2(1.125f + 0.25f * float(gid), 0.375f);
	out[gid] = t.sample(::s, uv);
}
