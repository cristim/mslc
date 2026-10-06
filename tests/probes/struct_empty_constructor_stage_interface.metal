// EXPECT: valid
// DISASM: OpDecorate %gl_Position BuiltIn Position
//
// A struct with a constructor is still usable as a stage_in struct and as a
// vertex output and a fragment input.
struct In { In() {} float2 p [[attribute(0)]]; };
struct V { V() {} float4 pos [[position]]; float2 uv; };
vertex V vs(In in [[stage_in]]) { V v; v.pos = float4(in.p, 0, 1); v.uv = in.p; return v; }
fragment float4 fs(V v [[stage_in]]) { return float4(v.uv, 0, 1); }
