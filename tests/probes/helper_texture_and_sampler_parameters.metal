// EXPECT: valid
// A helper takes a texture and a sampler by value, and the call hands over the caller's own.
// The parameters are pointers to the UniformConstant variables, loaded where they are used.
// DISASM-MATCH: OpTypeFunction %v4float %_ptr_UniformConstant_[0-9]+ %_ptr_UniformConstant_[0-9]+ %v2float
// DISASM-MATCH: OpFunctionParameter %_ptr_UniformConstant_[0-9]+
// DISASM-MATCH: OpFunctionCall %v4float %[0-9]+ %[0-9]+ %[0-9]+ %[0-9]+
#include <metal_stdlib>
using namespace metal;
float4 look(texture2d<float> t, sampler s, float2 uv) { return t.sample(s, uv); }
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return look(t, s, float2(0.25));
}
