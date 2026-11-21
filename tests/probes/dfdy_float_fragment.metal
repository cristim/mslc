// EXPECT: valid
// DISASM: OpDPdy %float
// DISASM-NOT: OpFConvert
struct In { float4 position [[position]]; };
fragment float4 main(In p [[stage_in]]) { float d = dfdy(p.position.y); return float4(d); }
