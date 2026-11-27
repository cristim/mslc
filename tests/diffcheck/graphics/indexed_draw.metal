// RUN: vertex=vs fragment=fs instances=2 base_vertex=3 base_instance=5 cases=2
// DIFFCHECK-POSITION: pos
// DIFFCHECK-GEOMETRY: 3,4 25,4 25,27 3,26
// DIFFCHECK-INDEXES: 1 2 0 3 0 2
// Indexed four-vertex quad drawn as two triangles with non-zero base_vertex and base_instance, distinct
// provoking vertices (1 and 3), and instance shift so both instances stay visible in the output.
#include <metal_stdlib>
using namespace metal;
struct VIn { float2 pos [[attribute(0)]]; uint tag [[attribute(1)]]; };
struct VOut { float4 pos [[position]]; uint4 val [[flat]]; };
vertex VOut vs(VIn in [[stage_in]], uint vid [[vertex_id]], uint iid [[instance_id]], constant uint *tbl [[buffer(1)]]) {
	VOut o;
	o.pos = float4(in.pos.x + 0.125f * float(iid - 5u), in.pos.y, 0.5f, 1.0f);
	o.val = uint4(vid, iid, in.tag ^ tbl[vid], tbl[iid + 16u]);
	return o;
}
fragment uint4 fs(VOut in [[stage_in]]) { return in.val; }
