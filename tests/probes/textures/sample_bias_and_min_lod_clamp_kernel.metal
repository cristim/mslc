// EXPECT: valid
// DISASM: OpImageSampleExplicitLod
// DISASM-NOT: OpImageSampleImplicitLod
// DISASM-NOT: Bias
// DISASM-NOT: MinLod
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Lod %float_1(_[0-9]+)?[^_0-9A-Za-z]
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Lod %float_0_5[0-9_]*[^_0-9A-Za-z]
// Measured on Apple M1 Pro GPU: in a kernel bias(b) with min_lod_clamp(m) ignores min_lod_clamp and reads level b.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]],
              device float4* o [[buffer(0)]], constant float& b [[buffer(1)]]) {
  o[0] = a.sample(s, float2(0.25), bias(1.0), min_lod_clamp(2.0));
  o[1] = a.sample(s, float2(0.25), bias(0.5));
  o[2] = a.sample(s, float2(0.25), bias(-1.0));
  o[3] = a.sample(s, float2(0.25), bias(b), min_lod_clamp(2.0));
}
