// EXPECT: valid
// DISASM: OpCapability MinLod
// DISASM-MATCH: OpImageSampleImplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Bias %float_1
// DISASM-MATCH: OpImageSampleImplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ MinLod %float_2
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Lod %float_1
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Lod %float_0
// A helper called from both fragment and vertex/kernel emits per-stage copies: fragment keeps
// OpImageSampleImplicitLod with Bias/MinLod, while vertex and kernel lower bias to Lod and drop min_lod_clamp.
#include <metal_stdlib>
using namespace metal;
float4 look(texture2d<float> a, sampler s) {
  return a.sample(s, float2(0.25), bias(1.0)) + a.sample(s, float2(0.25), min_lod_clamp(2.0));
}
struct VOut { float4 pos [[position]]; };
vertex VOut vs(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]]) {
  VOut o; o.pos = look(a, s); return o;
}
fragment float4 fs(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]]) {
  return look(a, s);
}
kernel void ks(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]]) {
  o[0] = look(a, s);
}
