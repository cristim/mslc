// RUN: vertex=vs fragment=fs cases=2
// DIFFCHECK-POSITION: pos
// DIFFCHECK: depth
// The fragment writes its own depth from an interpolated value; the depth target is read back
// and compared with the colour.
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
struct FOut { float4 c [[color(0)]]; float d [[depth(any)]]; };
fragment FOut fs(VOut in [[stage_in]]) {
	FOut o;
	o.c = in.col;
	o.d = 0.25 + 0.0625 * in.col.x;
	return o;
}
