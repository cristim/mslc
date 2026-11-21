// EXPECT: valid
// DISASM: DepthReplacing
// DISASM: DepthLess
// DISASM: DepthGreater
// DISASM-ENTRY-MODES: fa OriginUpperLeft, DepthReplacing, SignedZeroInfNanPreserve 32
// DISASM-ENTRY-MODES: fl OriginUpperLeft, DepthReplacing, DepthLess, SignedZeroInfNanPreserve 32
// DISASM-ENTRY-MODES: fg OriginUpperLeft, DepthReplacing, DepthGreater, SignedZeroInfNanPreserve 32
// DISASM-ENTRY-MODES: fc OriginUpperLeft, SignedZeroInfNanPreserve 32
struct A { float d [[depth(any)]]; };
struct L { float d [[depth(less)]]; };
struct G { float d [[depth(greater)]]; };
fragment A fa() { A o; o.d = 0.25; return o; }
fragment L fl() { L o; o.d = 0.25; return o; }
fragment G fg() { G o; o.d = 0.75; return o; }
fragment float4 fc() { return float4(0.2); }
