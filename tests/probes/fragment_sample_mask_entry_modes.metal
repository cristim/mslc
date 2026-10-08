// EXPECT: valid
// DISASM: BuiltIn SampleMask
// DISASM: DepthLess
// DISASM: DepthGreater
// DISASM-NOT: SampleRateShading
struct M { uint m [[sample_mask]]; };
struct L { float d [[depth(less)]]; uint m [[sample_mask]]; };
struct G { uint m [[sample_mask]]; float d [[depth(greater)]]; };
struct D { float d [[depth(any)]]; };
struct V { float4 p [[position]]; };
fragment M fa() { M o; o.m = 5u; return o; }
fragment M fb() { M o; o.m = 10u; return o; }
fragment L fl() { L o; o.d = 0.25; o.m = 5u; return o; }
fragment G fg() { G o; o.m = 10u; o.d = 0.75; return o; }
fragment D fd() { D o; o.d = 0.25; return o; }
fragment float4 fc() { return float4(0.75); }
vertex V v() { V o; o.p = float4(0.0); return o; }
