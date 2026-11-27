// RUN: vertex=vs fragment=fs cases=2
// DIFFCHECK-POSITION: pos
// Non-contiguous colour outputs (color 1, 3, 6 with gaps at 0, 2, 4, 5) driven by
// center_no_perspective, centroid_perspective and centroid_no_perspective varyings.
#include <metal_stdlib>
using namespace metal;
struct VIn {
	float4 pos [[attribute(0)]];
	float4 uv [[attribute(1)]];
	float2 bias [[attribute(2)]];
};
struct VOut {
	float4 pos [[position]];
	float2 np [[center_no_perspective]];
	float2 cp [[centroid_perspective]];
	float2 cnp [[centroid_no_perspective]];
	uint tag [[flat]];
};
vertex VOut vs(VIn in [[stage_in]], uint vid [[vertex_id]]) {
	VOut o;
	// Vary clip w by powers of two per vertex so perspective vs noperspective diverge without losing 1/256 grid precision.
	float w = float(1u << (vid % 3u));
	o.pos = float4(in.pos.xy * w, 0.5 * w, w);
	o.np = in.uv.xy * 0.5 + in.bias;
	o.cp = in.uv.zw * 0.25;
	o.cnp = in.uv.xy - in.bias * 0.5;
	o.tag = vid + 10u;
	return o;
}
struct FOut {
	float4 c1 [[color(1)]];
	half2 c3 [[color(3)]];
	uint2 c6 [[color(6)]];
};
fragment FOut fs(VOut in [[stage_in]]) {
	FOut o;
	o.c1 = float4(in.np, in.cp) + float4(1.0, 0.5, 0.25, 2.0);
	o.c3 = half2(in.cnp * 0.5) + half2(0.5h, 1.5h);
	o.c6 = uint2(in.tag, in.tag * 3u + 1u);
	return o;
}
