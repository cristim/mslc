// EXPECT: valid
// DISASM: BuiltIn FragDepth
// DISASM: OpStore %gl_FragDepth
// DISASM-MATCH: OpDecorate %[0-9]+ Location 0
// DISASM-NOT: OpDecorate %gl_FragDepth Location
struct O { float4 c [[color(0)]]; float d [[depth(any)]]; };
fragment O f() { O o; o.c = float4(0.2); o.d = 0.25; return o; }
