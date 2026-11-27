// RUN: grid=16 local=16 cases=3
// Verify constexpr and file-scope constant component variables select the matching channel in gather().
#include <metal_stdlib>
using namespace metal;
constant constexpr component gz = component::z;
kernel void k(texture2d<float> a [[texture(0)]],
	sampler s [[sampler(0)]],
	device float4* o [[buffer(0)]],
	uint t [[thread_position_in_grid]]) {
	float2 uv = float2(float(t % 4) + 0.5f, float(t / 4) + 0.5f) / 4.0f;
	constexpr component cx = component::x;
	const component cy = component::y;
	constexpr component cw = static_cast<component>(3);
	o[t] = a.gather(s, uv, int2(0), cx)
		+ a.gather(s, uv, int2(0), cy) * 2.0f
		+ a.gather(s, uv, int2(0), gz) * 3.0f
		+ a.gather(s, uv, int2(0), cw) * 4.0f;
}
