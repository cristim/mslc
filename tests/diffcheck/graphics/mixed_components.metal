// RUN: vertex=vs fragment=fs cases=3
// DIFFCHECK-POSITION: pos
// One target mixing interpolated components (0, 3), a constant (1) and a flat varying (2). The
// tolerance is per component, decided from Apple's image: the constant and the flat one are
// compared bit for bit, so a one-ULP error in them is a mismatch even though the file also
// has interpolated outputs.
#include <metal_stdlib>
using namespace metal;
struct VIn { float4 pos [[attribute(0)]]; float4 col [[attribute(1)]]; float tag [[attribute(2)]]; };
struct VOut { float4 pos [[position]]; float4 col; float tag [[flat]]; };
vertex VOut vs(VIn in [[stage_in]]) {
	VOut o;
	o.pos = in.pos;
	o.col = in.col;
	o.tag = in.tag;
	return o;
}
fragment float4 fs(VOut in [[stage_in]]) { return float4(in.col.x, 0.375, in.tag, in.col.z + 1.0); }
