// RUN: grid=48 local=16 cases=1
// DIFFCHECK-MIPS: s2 3
// DIFFCHECK-MIPS: r2 3
// DIFFCHECK-MIPS: r3 3
// DIFFCHECK-MIPS: rc 3
// DIFFCHECK-ULP: olin 16
// Multi-mip textures in kernel mode: sample with level(lod), bias(b), gradient2d(gx, gy),
// min_lod_clamp(m), embedded mip_filter::nearest/linear samplers, access::read(coord, lod)
// across 2d/3d/cube, and get_width/get_height/get_depth(lod) + get_num_mip_levels().
// cases=1 uses moderate (finite texels): linear mip blending of wide/edge float values can overflow or cancel differently.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> s2 [[texture(0)]],
              texture2d<uint, access::read> r2 [[texture(1)]],
              texture3d<float, access::read> r3 [[texture(2)]],
              texturecube<float, access::read> rc [[texture(3)]],
              sampler smp [[sampler(0)]],
              device float4 *os [[buffer(0)]],
              device float4 *or3 [[buffer(1)]],
              device float4 *orc [[buffer(2)]],
              device float4 *olin [[buffer(3)]],
              device uint4 *oru [[buffer(4)]],
              device uint4 *dims [[buffer(5)]],
              uint i [[thread_position_in_grid]]) {
	constexpr sampler em_near(address::clamp_to_edge, filter::nearest, mip_filter::nearest);
	constexpr sampler em_lin(address::clamp_to_edge, filter::nearest, mip_filter::linear);
	uint lod = i % 3u;
	uint mw = s2.get_width(lod);
	uint mh = s2.get_height(lod);
	uint x = i & 3u, y = (i >> 2) & 3u, face = (i >> 2) % 6u, z = i & 1u;
	float2 uv = (float2(x % mw, y % mh) + 0.5f) / float2(float(mw), float(mh));
	float g = (i & 1u) ? 0.5f : 1.0f;
	float4 gradSample = s2.sample(smp, float2(0.125f, 0.625f), gradient2d(float2(g, 0.0f), float2(0.0f, g)));
	float4 clampSample = s2.sample(smp, float2(0.125f, 0.625f), gradient2d(float2(0.25f, 0.0f), float2(0.0f, 0.25f)), min_lod_clamp(2.0f));
	os[i] = (i & 3u) == 0u ? s2.sample(smp, uv, level(float(lod)))
	      : (i & 3u) == 1u ? s2.sample(smp, float2(0.125f, 0.625f), bias(float(lod)))
	      : (i & 3u) == 2u ? gradSample
	      : (i & 4u) ? clampSample
	      : s2.sample(em_near, float2(0.625f, 0.375f), level(float(lod)));
	or3[i] = r3.read(uint3(x % r3.get_width(lod), y % r3.get_height(lod), z % r3.get_depth(lod)), lod);
	orc[i] = rc.read(uint2(x % rc.get_width(lod), y % rc.get_height(lod)), face, lod);
	olin[i] = s2.sample(em_lin, float2(0.625f, 0.375f), level(1.5f));
	oru[i] = r2.read(uint2(x % r2.get_width(lod), y % r2.get_height(lod)), lod);
	dims[i] = uint4(s2.get_num_mip_levels(), r3.get_width(lod), r3.get_depth(lod), rc.get_width(lod));
}
