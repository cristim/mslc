// RUN: vertex=vs fragment=fs cases=3
// DIFFCHECK-POSITION: pos
// A seeded per-vertex colour interpolated across each triangle, through a half-scale
// multiply so the vertex stage matters too. Compared within the interpolation tolerance.
#include <metal_stdlib>
using namespace metal;
struct VIn { float4 pos [[attribute(0)]]; float4 col [[attribute(1)]]; };
struct VOut { float4 pos [[position]]; float4 col; };
vertex VOut vs(VIn in [[stage_in]]) {
	VOut o;
	o.pos = in.pos;
	o.col = in.col * 0.5;
	return o;
}
fragment float4 fs(VOut in [[stage_in]]) { return in.col + float4(1.0, 0.0, 2.0, 0.0); }
