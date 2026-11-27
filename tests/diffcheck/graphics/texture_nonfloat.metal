// RUN: vertex=vs fragment=fs cases=3
// DIFFCHECK-POSITION: pos
// Non-float 2D textures (half, int, uint, short, ushort) in graphics mode with sample and
// access::read. Each texture is sampled or read at its own texel coordinate and weighted so
// wrong formats, swapped bindings or swapped coordinates change a colour target.
#include <metal_stdlib>
using namespace metal;
struct VIn { float4 pos [[attribute(0)]]; };
struct VOut { float4 pos [[position]]; };
vertex VOut vs(VIn in [[stage_in]]) {
	VOut o;
	o.pos = in.pos;
	return o;
}
struct FOut { half4 c0 [[color(0)]]; int4 c1 [[color(1)]]; uint4 c2 [[color(2)]]; };
fragment FOut fs(texture2d<half, access::read> thr [[texture(0)]],
                 texture2d<half> ths [[texture(1)]],
                 texture2d<int, access::read> ti [[texture(2)]],
                 texture2d<short, access::read> ts [[texture(3)]],
                 texture2d<uint> tu [[texture(4)]],
                 texture2d<ushort> tus [[texture(5)]],
                 sampler s [[sampler(0)]]) {
	FOut o;
	o.c0 = thr.read(uint2(1, 2)) + ths.sample(s, float2(0.375, 0.625)) * half4(0.5h, 1.0h, 0.25h, 0.125h);
	o.c1 = ti.read(uint2(2, 1)) * int4(1, 2, 3, 4) + int4(ts.read(uint2(0, 3))) * int4(5, 7, 11, 13);
	o.c2 = tu.sample(s, float2(0.875, 0.375)) * uint4(1u, 3u, 5u, 7u) + uint4(tus.sample(s, float2(0.125, 0.625))) * uint4(11u, 13u, 17u, 19u);
	return o;
}
