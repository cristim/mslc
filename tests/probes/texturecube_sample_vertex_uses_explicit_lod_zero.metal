// EXPECT: valid
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Lod %float_0(_[0-9])?[^_0-9a-zA-Z]
// DISASM-NO-MATCH: OpImageSampleImplicitLod
#include <metal_stdlib>
using namespace metal;
struct V { float4 position [[position]]; };
vertex V v(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]])
{ V o; o.position = t.sample(s, float3(1.0, 0.0, 0.0)); return o; }
