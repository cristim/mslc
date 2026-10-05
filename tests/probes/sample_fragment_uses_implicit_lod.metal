// EXPECT: valid
// A fragment function takes the level from the derivatives, which is the implicit-lod
// instruction. Combined as OpSampledImage from the loaded image and the loaded sampler.
// DISASM-MATCH: OpImageSampleImplicitLod %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+[^_0-9a-zA-Z]
// DISASM-NO-MATCH: OpImageSampleExplicitLod
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.25));
}
