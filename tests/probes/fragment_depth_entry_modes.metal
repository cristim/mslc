// EXPECT: valid
// DISASM: DepthReplacing
// DISASM: DepthLess
// DISASM: DepthGreater
struct A { float d [[depth(any)]]; };
struct L { float d [[depth(less)]]; };
struct G { float d [[depth(greater)]]; };
fragment A fa() { A o; o.d = 0.25; return o; }
fragment L fl() { L o; o.d = 0.25; return o; }
fragment G fg() { G o; o.d = 0.75; return o; }
fragment float4 fc() { return float4(0.2); }
