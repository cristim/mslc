// EXPECT: valid
// DISASM: OpFwidth %v2float
struct In { float4 position [[position]]; };
fragment float4 main(In p [[stage_in]]) { float2 width = fwidth(float2(p.position.x)); return float4(width.x); }
