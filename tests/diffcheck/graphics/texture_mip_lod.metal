// RUN: vertex=vs fragment=fs cases=2
// DIFFCHECK-POSITION: pos
// DIFFCHECK-MIPS: vt 3
// DIFFCHECK-MIPS: vr 3
// DIFFCHECK-MIPS: ft 3
// DIFFCHECK-MIPS: fr 3
// DIFFCHECK-ULP: color2 16
// Multi-mip textures across vertex and fragment stages: sample with level(lod), bias(b), gradient2d(gx, gy),
// min_lod_clamp(m), embedded mip_filter::nearest/linear samplers, access::read(coord, lod), and mip size queries.
#include <metal_stdlib>
using namespace metal;
struct VIn { float4 pos [[attribute(0)]]; };
struct VOut { float4 pos [[position]]; uint tri [[flat]]; float4 vsamp [[flat]]; uint4 vread [[flat]]; };
vertex VOut vs(VIn in [[stage_in]], uint vid [[vertex_id]],
               texture2d<float> vt [[texture(0)]],
               texture2d<uint, access::read> vr [[texture(1)]],
               sampler sv [[sampler(0)]]) {
	VOut o;
	o.pos = in.pos;
	uint tri = vid / 3u;
	uint lod = tri % 3u;
	float flod = float(lod);
	o.tri = tri;
	o.vsamp = (tri & 1u) ? vt.sample(sv, float2(0.125f, 0.625f), level(flod))
	                     : vt.sample(sv, float2(0.625f, 0.375f), bias(flod));
	o.vread = vr.read(uint2(0u, 0u), lod) + uint4(vt.get_num_mip_levels(), vr.get_width(lod), vr.get_height(lod), 0u);
	return o;
}
struct FOut { float4 c0 [[color(0)]]; uint4 c1 [[color(1)]]; float4 c2 [[color(2)]]; float4 c3 [[color(3)]]; };
fragment FOut fs(VOut in [[stage_in]],
                 texture2d<float> ft [[texture(0)]],
                 texture2d<uint, access::read> fr [[texture(1)]],
                 sampler sf [[sampler(0)]]) {
	constexpr sampler em_near(address::clamp_to_edge, filter::nearest, mip_filter::nearest);
	constexpr sampler rep_bias(address::repeat, filter::nearest, mip_filter::nearest);
	constexpr sampler em_lin(address::clamp_to_edge, filter::nearest, mip_filter::linear);
	FOut o;
	uint lod = in.tri % 3u;
	float4 fgrad = ft.sample(sf, float2(0.125f, 0.625f), gradient2d(float2(0.5f, 0.0f), float2(0.0f, 0.5f)));
	float4 fclamp = ft.sample(sf, float2(0.125f, 0.625f), gradient2d(float2(0.25f, 0.0f), float2(0.0f, 0.25f)), min_lod_clamp(2.0f));
	float4 fnear = ft.sample(em_near, float2(0.625f, 0.375f), level(2.0f));
	o.c0 = in.vsamp + (in.tri <= 1u ? ft.sample(sf, float2(0.125f, 0.625f), level(float(lod)))
	                 : in.tri == 2u ? fgrad
	                 : in.tri == 3u ? fclamp
	                 : fnear);
	o.c1 = in.vread + fr.read(uint2(0u, 0u), lod) + uint4(ft.get_num_mip_levels(), fr.get_width(lod), fr.get_height(lod), in.tri);
	o.c2 = ft.sample(em_lin, float2(0.625f, 0.375f), level(1.5f));
	o.c3 = ft.sample(rep_bias, float2(in.pos.x * 0.25f, 0.375f), bias(1.0f));
	return o;
}
