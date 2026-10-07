// EXPECT: valid
// A half texture parameter is a half texture: the sample is narrowed inside the helper.
// DISASM: OpFConvert
#include <metal_stdlib>
using namespace metal;
half4 look(texture2d<half> t, sampler s) { return t.sample(s, float2(0.25)); }
fragment half4 f(texture2d<half> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return look(t, s);
}
