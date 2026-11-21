// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
struct V { float4 p [[position]]; };
vertex V v(uint vid [[vertex_id]], uint iid [[instance_id]]) { V r; r.p = float4(0); (void)vid; (void)iid; return r; }
