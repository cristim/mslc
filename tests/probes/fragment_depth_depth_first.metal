// EXPECT: valid
// DISASM: BuiltIn FragDepth
// DISASM: OpStore %gl_FragDepth
// DISASM-MATCH: OpDecorate %[0-9]+ Location 0
// DISASM-NOT: OpDecorate %gl_FragDepth Location
struct O { float d [[depth(any)]]; float4 c [[color(0)]]; };
fragment O f() { O o; o.d = 0.25; o.c = float4(0.2); return o; }
