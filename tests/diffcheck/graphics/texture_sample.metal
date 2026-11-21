// RUN: vertex=vs fragment=fs cases=3
// DIFFCHECK-POSITION: pos
// A texture sampled at a fixed texel centre (0.375, 0.625 of a 4x4 texture, so no filter or
// rounding edge) added to an interpolated colour; a wrong texture binding, sampler or
// coordinate order changes the result.
#include <metal_stdlib>
using namespace metal;
struct VIn { float4 pos [[attribute(0)]]; float4 col [[attribute(1)]]; };
struct VOut { float4 pos [[position]]; float4 col; };
vertex VOut vs(VIn in [[stage_in]]) {
	VOut o;
	o.pos = in.pos;
	o.col = in.col;
	return o;
}
fragment float4 fs(VOut in [[stage_in]], texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
	return in.col + t.sample(s, float2(0.375, 0.625)) * float4(1.0, 2.0, 3.0, 4.0);
}
