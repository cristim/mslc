// EXPECT: valid
// DISASM: OpKill
// DISASM-ENTRY-STORES-PER-RETURN: f SampleMask
#include <metal_stdlib>
using namespace metal;
struct O { uint m [[sample_mask]]; };
struct I { float4 p [[position]]; };
fragment O f(I i [[stage_in]]) {
    O o; o.m = 3u;
    if (i.p.x < 1.0) discard_fragment();
    if (i.p.y < 1.0) { return o; }
    o.m = 5u; return o;
}
