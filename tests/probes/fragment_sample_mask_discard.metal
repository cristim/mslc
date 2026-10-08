// EXPECT: valid
// DISASM: BuiltIn SampleMask
// DISASM: OpKill
#include <metal_stdlib>
using namespace metal;
struct O { uint m [[sample_mask]]; };
struct I { float4 p [[position]]; };
fragment O f(I i [[stage_in]]) { if (i.p.x < 1.0) discard_fragment(); O o; o.m = 0xffffffffu; return o; }
