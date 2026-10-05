// EXPECT: valid
// level(x) is the Lod image operand, in a fragment function as well, since the
// implicit form takes its level from the derivatives. A float literal reaches the
// instruction as it is.
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Lod %float_2(_[0-9])?[^_0-9a-zA-Z]
// DISASM-NO-MATCH: OpImageSampleImplicitLod
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.25), level(2.0));
}
