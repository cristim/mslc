// EXPECT: valid
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Lod %float_2[^_0-9a-zA-Z]
// DISASM-NO-MATCH: OpImageSampleImplicitLod
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float3(1.0, 0.0, 0.0), level(2.0));
}
