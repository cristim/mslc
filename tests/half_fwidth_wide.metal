// RUN: vertex=vs fragment=fs cases=1
// DIFFCHECK-POSITION: pos
// fwidth of a half whose |dx| is not a half (#173), also used by tests/half_fwidth_expected_test.py.
// 1024 sits next to 2^-10 in a quad row, so |dx| = 1024 - 2^-10 needs more than 11 significand bits;
// adding |dy| = 1.5 gives 1026 when each term is narrowed to half first (Apple, and mslc) and 1025 when
// the float32 sum is narrowed once.
#include <metal_stdlib>
using namespace metal;
struct VIn { float4 pos [[attribute(0)]]; };
struct VOut { float4 pos [[position]]; };
vertex VOut vs(VIn in [[stage_in]]) { VOut o; o.pos = in.pos; return o; }
fragment half4 fs(VOut in [[stage_in]]) {
	uint xi = uint(in.pos.x), yi = uint(in.pos.y);
	half wide = half((xi & 1u) != 0u ? 1024.0 : 0.0009765625 + float(yi & 1u) * 1.5);
	return half4(fwidth(wide), half(0.0), half(0.0), half(0.0));
}
