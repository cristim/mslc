// EXPECT: valid
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Lod %float_0(_[0-9]+)?[^_0-9A-Za-z]
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Lod %float_1(_[0-9]+)?[^_0-9A-Za-z]
// DISASM-NOT: OpImageSampleImplicitLod
// DISASM-NOT: MinLod
// DISASM-NOT: Bias
// Measured on Apple M1 Pro GPU: in a vertex function min_lod_clamp without gradients is ignored
// (alone reads level 0, with bias(b) reads level b).
#include <metal_stdlib>
using namespace metal;
float4 vHelper(texture2d<float> a, sampler s, float m) {
  return a.sample(s, float2(0.25), min_lod_clamp(m));
}
struct VOut { float4 pos [[position]]; float4 c [[user(locn0)]]; };
vertex VOut v(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], constant float& m [[buffer(0)]]) {
  VOut o;
  o.pos = a.sample(s, float2(0.25), min_lod_clamp(2.0));
  o.c = vHelper(a, s, m);
  o.c += a.sample(s, float2(0.25), bias(1.0), min_lod_clamp(2.0));
  return o;
}
