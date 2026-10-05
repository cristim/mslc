// EXPECT: valid
// The float texture's lookup is the result itself, with no conversion in between.
// DISASM-NO-MATCH: OpFConvert
// DISASM-MATCH: OpSampledImage %_?[_0-9a-zA-Z]* %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.25));
}
