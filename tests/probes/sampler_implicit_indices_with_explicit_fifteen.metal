// EXPECT: valid
// One sampler claims 15 and fifteen unattributed ones take 0 to 14: Apple accepts it.
// REFLECT: { "kind": "Sampler", "metal_index": 14, "descriptor": { "set": 1, "binding": 15 }, "param_index": 15, "name": "s14" }
// REFLECT: "metal_index": 15,
#include <metal_stdlib>
using namespace metal;
fragment float4 f(sampler s15 [[sampler(15)]], sampler s0, sampler s1, sampler s2, sampler s3, sampler s4, sampler s5, sampler s6, sampler s7, sampler s8, sampler s9, sampler s10, sampler s11, sampler s12, sampler s13, sampler s14) { return float4(0.0); }
