// EXPECT: valid
// A kernel has no derivatives either, so it samples level 0 explicitly.
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Lod %float_0(_[0-9])?[^_0-9a-zA-Z]
// DISASM-NO-MATCH: OpImageSampleImplicitLod
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]], device float4* out [[buffer(0)]])
{ out[0] = t.sample(s, float2(0.25)); }
