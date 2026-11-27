// EXPECT: valid
// DISASM: OpFwidth %float
struct In { float4 position [[position]]; };
float footprint(float value) { const float width = fwidth(value); return width; }
fragment float4 main(In p [[stage_in]]) { return float4(footprint(2.0f*p.position.x + 3.0f*p.position.y)); }
