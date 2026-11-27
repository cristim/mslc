// EXPECT: valid
// Apple (vertex): stage_in takes no buffer index, so b is 0, a is 1 and c is 2.
// REFLECT: "metal_index": 1, "descriptor": { "set": 0, "binding": 0 }, "member": 0, "param_index": 1, "name": "a"
// REFLECT: "metal_index": 0, "descriptor": { "set": 0, "binding": 0 }, "member": 1, "param_index": 2, "name": "b"
// REFLECT: "metal_index": 2, "descriptor": { "set": 0, "binding": 0 }, "member": 2, "param_index": 3, "name": "c"
#include <metal_stdlib>
using namespace metal;
struct V { float4 p [[attribute(0)]]; };
struct O { float4 p [[position]]; };
vertex O v(V in [[stage_in]], constant float4 &a, device float *b [[buffer(0)]], device float *c, uint vid [[vertex_id]]) { O o; o.p = in.p + a + b[0] + c[0]; return o; }
