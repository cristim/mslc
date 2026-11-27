// RUN: vertex=vs fragment=fs cases=3
// DIFFCHECK-POSITION: pos
// Scalar and vector half stage_in attributes (half, half2, half3, half4): mslc widens them
// to 32-bit float SPIR-V Inputs and narrows back with OpFConvert while both hosts feed the
// 16-bit vertex format bytes.
#include <metal_stdlib>
using namespace metal;
struct VIn {
	float2 pos [[attribute(0)]];
	half h1 [[attribute(1)]];
	half2 h2 [[attribute(2)]];
	half3 h3 [[attribute(3)]];
	half4 h4 [[attribute(4)]];
};
struct VOut { float4 pos [[position]]; half4 a [[flat]]; half4 b [[flat]]; };
vertex VOut vs(VIn in [[stage_in]]) {
	VOut o;
	o.pos = float4(in.pos, 0.5f, 1.0f);
	o.a = half4(in.h1 * 0.5h, in.h2, in.h3.x);
	o.b = half4(in.h3.yz, in.h4.xy) + half4(in.h4.zw, in.h1 + 0.5h, 1.5h);
	return o;
}
struct FOut { half4 c0 [[color(0)]]; half4 c1 [[color(1)]]; };
fragment FOut fs(VOut in [[stage_in]]) {
	FOut o;
	o.c0 = in.a + half4(0.25h, 0.5h, 1.0h, 2.0h);
	o.c1 = in.b;
	return o;
}
