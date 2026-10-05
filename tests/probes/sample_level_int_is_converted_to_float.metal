// EXPECT: valid
// Apple takes level(1) and converts the int; the Lod operand has to be a float.
// DISASM-MATCH: OpConvertSToF %float %[_0-9a-zA-Z]+[^_0-9a-zA-Z]
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Lod %[_0-9a-zA-Z]+
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  int l = 1;
  return t.sample(s, float2(0.25), level(l));
}
