// RUN: vertex=vs fragment=fs cases=2
// DIFFCHECK-POSITION: pos
// Integer render targets and a flat integer varying: bit-exact. The vertex id reaches the
// fragment through the provoking vertex, so a flat/interpolated mix-up or a wrong
// provoking vertex changes the result.
#include <metal_stdlib>
using namespace metal;
struct VIn { float2 pos [[attribute(0)]]; int2 tag [[attribute(1)]]; };
struct VOut { float4 pos [[position]]; uint id [[flat]]; int2 tag [[flat]]; };
vertex VOut vs(VIn in [[stage_in]], uint vid [[vertex_id]]) {
	VOut o;
	o.pos = float4(in.pos, 0.5, 1.0);
	o.id = vid;
	o.tag = in.tag;
	return o;
}
struct FOut { uint4 u [[color(0)]]; int4 i [[color(1)]]; };
fragment FOut fs(VOut in [[stage_in]]) {
	FOut o;
	o.u = uint4(in.id, in.id * 3u + 1u, 7u, 9u);
	o.i = int4(in.tag, -5, 11);
	return o;
}
