// EXPECT: valid
// Two helpers whose parameter lists differ only in the image type get two function types.
// DISASM-MATCH: OpTypeFunction %v4float %_ptr_UniformConstant_[0-9]+ %_ptr_UniformConstant_[0-9]+
#include <metal_stdlib>
using namespace metal;
float4 flat(texture2d<float> t, sampler s) { return t.sample(s, float2(0.25)); }
float4 cube(texturecube<float> t, sampler s) { return t.sample(s, float3(0.25)); }
fragment float4 f(texture2d<float> a [[texture(0)]], texturecube<float> b [[texture(1)]], sampler s [[sampler(0)]]) {
  return flat(a, s) + cube(b, s);
}
