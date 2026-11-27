// EXPECT: valid
// DISASM: OpFConvert %v4float
// DISASM: OpDPdx %v4float
// DISASM: OpFConvert %v4half
//
// A half argument is widened around the derivative and narrowed again, as fwidth does.
struct In { half4 value [[user(locn0)]]; float4 position [[position]]; };
fragment half4 main(In p [[stage_in]]) { return dfdx(p.value); }
