// EXPECT: valid
// A parameter is passed on to the next helper as the pointer it is.
// DISASM-MATCH: OpFunctionCall %v4float %[0-9]+ %[0-9]+ %[0-9]+ %[0-9]+
#include <metal_stdlib>
using namespace metal;
float4 inner(texture2d<float> t, sampler s, float2 uv) { return t.sample(s, uv); }
float4 outer(texture2d<float> t, sampler s, float2 uv) { return inner(t, s, uv) * 0.5f; }
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return outer(t, s, float2(0.25));
}
