// RUN: vertex=vs fragment=fs instances=3 cases=2
// DIFFCHECK-POSITION: pos
// Three instances, each shifted in x and tinted by a [[buffer]] table indexed by instance_id
// (a constant buffer read in the vertex stage). Later instances overdraw earlier ones, so
// the order and the instance index both show.
#include <metal_stdlib>
using namespace metal;
struct VIn { float2 pos [[attribute(0)]]; };
struct VOut { float4 pos [[position]]; float4 tint [[flat]]; };
vertex VOut vs(VIn in [[stage_in]], uint iid [[instance_id]], constant float4* tints [[buffer(1)]]) {
	VOut o;
	o.pos = float4(in.pos.x + 0.0625 * float(iid), in.pos.y, 0.5, 1.0);
	o.tint = tints[iid];
	return o;
}
fragment float4 fs(VOut in [[stage_in]]) { return in.tint; }
