// RUN: vertex=vs fragment=fs cases=3
// DIFFCHECK-POSITION: pos
// Non-2D textures (1d, 1d_array, 2d_array, 3d, cube, cube_array) across vertex and fragment
// stages with sample, access::read and dimension/array-size/depth queries.
#include <metal_stdlib>
using namespace metal;
struct VIn { float4 pos [[attribute(0)]]; };
struct VOut { float4 pos [[position]]; float4 c0 [[flat]]; int4 c1 [[flat]]; };
vertex VOut vs(VIn in [[stage_in]], uint vid [[vertex_id]],
               texture1d<float> s1 [[texture(0)]],
               texture1d_array<int, access::read> r1a [[texture(1)]],
               sampler sv [[sampler(0)]]) {
	VOut o;
	o.pos = in.pos;
	uint t = vid / 3u;
	uint x = t & 3u;
	o.c0 = s1.sample(sv, (float(x) + 0.5f) * 0.25f) * float4(1.0f, 0.5f, 0.25f, 2.0f);
	o.c1 = r1a.read(3u - x, t % 3u) + int4(int(s1.get_width()), int(r1a.get_array_size()), 0, 0);
	return o;
}
struct FOut { float4 c0 [[color(0)]]; uint4 c1 [[color(1)]]; half4 c2 [[color(2)]]; };
fragment FOut fs(VOut in [[stage_in]],
                 texture2d_array<uint, access::read> r2a [[texture(0)]],
                 texture3d<half, access::read> r3 [[texture(1)]],
                 texturecube<float> sc [[texture(2)]],
                 texturecube_array<float> sca [[texture(3)]],
                 sampler sf [[sampler(0)]]) {
	FOut o;
	o.c0 = in.c0
	     + sc.sample(sf, float3(1.0f, 0.25f, -0.25f)) * float4(0.5f, 1.0f, 0.25f, 0.125f)
	     + sca.sample(sf, float3(-0.25f, -1.0f, 0.25f), 1u) * float4(0.25f, 0.5f, 1.0f, 2.0f);
	o.c1 = uint4(in.c1)
	     + r2a.read(uint2(1u, 2u), 2u) * uint4(1u, 3u, 5u, 7u)
	     + uint4(r2a.get_height() + sc.get_width(), r2a.get_array_size(), r3.get_depth(), sca.get_array_size());
	o.c2 = r3.read(uint3(2u, 3u, 1u)) + half4(0.5h, 1.0h, 1.5h, 2.0h);
	return o;
}
