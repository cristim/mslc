// RUN: vertex=vs fragment=fs cases=3
// DIFFCHECK-POSITION: pos
// Vertex-stage sampled and access::read textures plus runtime samplers alongside fragment
// textures and samplers at overlapping Metal indices (set 0 vs set 1). Each stage uses
// distinct coordinates and weights so swapping stage bindings, sampler indices or texel
// coordinates changes the output.
#include <metal_stdlib>
using namespace metal;
struct VIn { float4 pos [[attribute(0)]]; };
struct VOut { float4 pos [[position]]; float4 vsamp [[flat]]; int4 vread [[flat]]; };
vertex VOut vs(VIn in [[stage_in]], uint vid [[vertex_id]],
               texture2d<float> vt [[texture(0)]],
               texture2d<int, access::read> vr [[texture(1)]],
               sampler s0 [[sampler(0)]],
               sampler s1 [[sampler(1)]]) {
	VOut o;
	o.pos = in.pos;
	o.vsamp = vt.sample(s0, float2(-0.375, 0.625)) + vt.sample(s1, float2(-0.375, 1.375)) * float4(1.0, 2.0, 3.0, 4.0);
	o.vread = vr.read(uint2(vid % 4, (vid / 4) % 4));
	return o;
}
struct FOut { float4 c0 [[color(0)]]; int4 c1 [[color(1)]]; };
fragment FOut fs(VOut in [[stage_in]],
               texture2d<float> ft [[texture(0)]],
               sampler s [[sampler(0)]]) {
	FOut o;
	o.c0 = in.vsamp + ft.sample(s, float2(-0.375f, 1.375f)) * float4(0.5, 0.25, 0.125, 0.0625);
	o.c1 = in.vread;
	return o;
}
