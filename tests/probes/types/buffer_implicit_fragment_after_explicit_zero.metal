// EXPECT: valid
// Apple (fragment): b=0, a=1, c=2; the texture and sampler number apart (0, 0).
// REFLECT: "metal_index": 1, "descriptor": { "set": 1, "binding": 0 }, "member": 0, "param_index": 1, "name": "a"
// REFLECT: "metal_index": 0, "descriptor": { "set": 1, "binding": 0 }, "member": 1, "param_index": 4, "name": "b"
// REFLECT: "metal_index": 2, "descriptor": { "set": 1, "binding": 0 }, "member": 2, "param_index": 5, "name": "c"
#include <metal_stdlib>
using namespace metal;
struct O { float4 p [[position]]; };
fragment float4 f(O in [[stage_in]], constant float4 &a, texture2d<float> t, sampler s, device float *b [[buffer(0)]], device float *c) { return t.sample(s, in.p.xy) + a + b[0] + c[0]; }
