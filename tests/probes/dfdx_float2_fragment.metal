// EXPECT: valid
// DISASM: OpDPdx %v2float
// DISASM-NOT: OpFConvert
//
// dfdx is the partial derivative along x of its argument, a fragment function
// builtin like fwidth.
struct In { float4 position [[position]]; };
fragment float4 main(In p [[stage_in]]) { float2 d = dfdx(float2(p.position.x)); return float4(d.x); }
