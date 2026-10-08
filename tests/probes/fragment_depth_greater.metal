// EXPECT: valid
// DISASM: BuiltIn FragDepth
// DISASM: DepthReplacing
// DISASM: OpTypeFloat 32
// DISASM: OpStore %gl_FragDepth
// DISASM-NOT: EarlyFragmentTests
// DISASM-NOT: OpDecorate %gl_FragDepth Location
// DISASM: DepthGreater
// DISASM-NOT: DepthLess
struct O { float d [[depth(greater)]]; };
fragment O f() { O o; o.d = 0.75; return o; }
