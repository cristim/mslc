// RUN: vertex=vs fragment=fs cases=2
// DIFFCHECK-POSITION: pos
// DIFFCHECK-ULP: color1 14
// Embedded constexpr samplers in both stages: nearest clamp_to_zero, repeat and mirrored_repeat
// outside [0, 1] (bit-exact in color0), plus a linear clamp_to_edge sampler at interior
// 1/1024-grid coordinates (14-ULP bound in color1).
#include <metal_stdlib>
using namespace metal;
struct VIn { float4 pos [[attribute(0)]]; };
struct VOut { float4 pos [[position]]; float4 vcol [[flat]]; };
vertex VOut vs(VIn in [[stage_in]], uint vid [[vertex_id]], texture2d<float> vt [[texture(0)]]) {
	constexpr sampler vmirror(address::mirrored_repeat, filter::nearest);
	VOut o;
	o.pos = in.pos;
	float u = float(int((vid / 3) % 3) - 1) * 0.5f + 0.125f;
	o.vcol = vt.sample(vmirror, float2(u, 1.375f));
	return o;
}
struct FOut { float4 c0 [[color(0)]]; float4 c1 [[color(1)]]; };
fragment FOut fs(VOut in [[stage_in]], texture2d<float> ft [[texture(0)]]) {
	constexpr sampler cz(address::clamp_to_zero, filter::nearest);
	constexpr sampler rep(address::repeat, filter::nearest);
	constexpr sampler lin(address::clamp_to_edge, mag_filter::linear, min_filter::linear);
	FOut o;
	o.c0 = in.vcol
	     + ft.sample(cz, float2(1.125f, 0.375f)) * 2.0f
	     + ft.sample(cz, float2(0.125f, 0.375f)) * 0.5f
	     + ft.sample(rep, float2(1.375f, -0.375f)) * 0.25f;
	o.c1 = ft.sample(lin, float2(307.0f / 1024.0f, 531.0f / 1024.0f));
	return o;
}
