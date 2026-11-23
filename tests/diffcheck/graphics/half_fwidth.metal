// RUN: vertex=vs fragment=fs cases=1
// DIFFCHECK-POSITION: pos
// fwidth of half values (#173): four halves built from the integer pixel position, spanning
// small to large magnitudes so the result exercises half rounding. Each fwidth is widened to
// float and scaled by 2^22 into a uint, so the compare is exact. Expected: Apple and mslc on
// lavapipe match bit for bit.
#include <metal_stdlib>
using namespace metal;
struct VIn { float4 pos [[attribute(0)]]; };
struct VOut { float4 pos [[position]]; };
vertex VOut vs(VIn in [[stage_in]]) { VOut o; o.pos = in.pos; return o; }
fragment uint4 fs(VOut in [[stage_in]]) {
	float x = floor(in.pos.x), y = floor(in.pos.y);
	half a = half(x * x * 0.37 + y * 0.123);
	half b = half(x * 0.0171 + y * y * 0.0049);
	half c = half(x * 3.3 + y * 7.7);
	half d = half(x * 211.0 + y * y * 13.1);
	half4 r = half4(fwidth(a), fwidth(b), fwidth(c), fwidth(d));
	return uint4(uint(float(r.x) * 4194304.0), uint(float(r.y) * 4194304.0),
	             uint(float(r.z) * 4194304.0), uint(float(r.w) * 4194304.0));
}
