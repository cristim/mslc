// EXPECT: valid
// DISASM: BuiltIn FragDepth
// DISASM: DepthReplacing
// DISASM: OpTypeFloat 32
// DISASM: OpStore %gl_FragDepth
// DISASM-NOT: EarlyFragmentTests
// DISASM-NOT: OpDecorate %gl_FragDepth Location
// DISASM-NOT: DepthLess
// DISASM-NOT: DepthGreater
struct O { float d [[depth(any)]]; };
fragment O f() { O o; o.d = 0.25; return o; }
