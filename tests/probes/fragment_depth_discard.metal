// EXPECT: valid
// DISASM: OpKill
// DISASM: OpStore %gl_FragDepth
#include <metal_stdlib>
using namespace metal;
struct O { float d [[depth(any)]]; };
struct I { float4 p [[position]]; };
fragment O f(I i [[stage_in]]) { if (i.p.x < 1.0) discard_fragment(); O o; o.d = 0.25; return o; }
