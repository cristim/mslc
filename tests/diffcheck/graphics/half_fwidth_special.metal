// RUN: vertex=vs fragment=fs cases=1
// DIFFCHECK-POSITION: pos
// fwidth of half values (#173) with a host-computed expected value (tests/half_fwidth_expected_test.py):
// rounding of the result, subnormals, an infinite input, a NaN input, inf - inf, and a finite input pair
// whose float sum overflows half. Every input is built from the integer pixel position with exact float
// arithmetic, so the expected half bits can be computed independently with numpy float16.
#include <metal_stdlib>
using namespace metal;
struct VIn { float4 pos [[attribute(0)]]; };
struct VOut { float4 pos [[position]]; };
vertex VOut vs(VIn in [[stage_in]]) { VOut o; o.pos = in.pos; return o; }
struct FOut { half4 r [[color(0)]]; half2 s [[color(1)]]; };
fragment FOut fs(VOut in [[stage_in]]) {
	uint xi = uint(in.pos.x), yi = uint(in.pos.y);
	float x = float(xi), y = float(yi);
	half rounded = half(x * 81.3125 + y * 219.0625);
	half tiny = half((x * 3.0 + y * 5.0) * 5.9604645e-08);
	half inf_in = xi == 6u ? half(INFINITY) : half(x + y);
	half nan_in = xi == 10u ? half(NAN) : half(x + y);
	half inf_inf = (xi == 14u || xi == 15u) ? half(INFINITY) : half(x + y);
	half over = half(float(xi & 1u) * 40000.0 - float(yi & 1u) * 40000.0);
	FOut o;
	o.r = half4(fwidth(rounded), fwidth(tiny), fwidth(inf_in), fwidth(nan_in));
	o.s = half2(fwidth(inf_inf), fwidth(over));
	return o;
}
