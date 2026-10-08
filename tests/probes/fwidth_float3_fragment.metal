// EXPECT: valid
// DISASM: OpFwidth %v3float
struct In { float4 position [[position]]; };
fragment float4 main(In p [[stage_in]]) { float3 width = fwidth(float3(p.position.x)); return float4(width.x); }
