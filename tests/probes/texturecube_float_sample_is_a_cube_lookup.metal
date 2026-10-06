// EXPECT: valid
// Dim Cube with a float sampled type, Sampled 1, depth unspecified, not arrayed: Iridium's
// OpTypeImage for texturecube (indium src/iridium/air.cpp:771-785). Combined with the sampler
// as OpSampledImage and sampled with a float3 direction, implicitly in a fragment function.
// DISASM: OpTypeImage %float Cube 2 0 0 1 Unknown
// DISASM-MATCH: OpTypeSampledImage %[_0-9a-zA-Z]+
// DISASM-MATCH: OpImageSampleImplicitLod %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+[^_0-9a-zA-Z]
// DISASM-NO-MATCH: OpImageSampleExplicitLod
// DISASM-NO-MATCH: OpFConvert
// DISASM-NO-MATCH: Dim2D
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float3(0.25, 0.5, 0.75));
}
