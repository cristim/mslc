// EXPECT: valid
// DISASM: OpFwidth %float
struct In { float4 position [[position]]; };
fragment float4 main(In p [[stage_in]]) { return float4(metal::fwidth(p.position.x)); }
