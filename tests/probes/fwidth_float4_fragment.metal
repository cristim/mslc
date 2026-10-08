// EXPECT: valid
// DISASM: OpFwidth %v4float
struct In { float4 position [[position]]; };
fragment float4 main(In p [[stage_in]]) { float4 width = fwidth(float4(p.position.x)); return float4(width.x); }
