// EXPECT: valid
// A sampler the app binds is not known to be normalized, so a fragment lookup through
// it stays the implicit form.
// DISASM-MATCH: OpImageSampleImplicitLod %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+[^_0-9a-zA-Z]
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.25));
}
