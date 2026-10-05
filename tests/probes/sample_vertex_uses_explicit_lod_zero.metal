// EXPECT: valid
// A vertex function has no derivatives, and spirv-val rejects an implicit-lod sample
// there. Metal samples level 0, so the lookup carries Lod 0.0.
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Lod %float_0(_[0-9])?[^_0-9a-zA-Z]
// DISASM-NO-MATCH: OpImageSampleImplicitLod
#include <metal_stdlib>
using namespace metal;
struct V { float4 position [[position]]; };
vertex V v(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]])
{ V o; o.position = t.sample(s, float2(0.25)); return o; }
