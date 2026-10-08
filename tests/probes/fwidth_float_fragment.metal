// EXPECT: valid
// DISASM: OpFwidth %float
// DISASM-NOT: GLSL.std.450
struct In { float4 position [[position]]; };
fragment float4 main(In p [[stage_in]]) { return float4(fwidth(p.position.x)); }
