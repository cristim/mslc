// EXPECT: valid
// DISASM: OpImageSampleExplicitLod
// DISASM-NOT: OpImageSampleImplicitLod
// DISASM-NOT: MinLod
// Measured on Apple M1 Pro GPU (4-level 8x8 RGBA32F): min_lod_clamp in a helper called from a kernel reads level 0.
#include <metal_stdlib>
using namespace metal;
float4 f(texture2d<float> a, sampler s) {
  return a.sample(s, float2(0.25), min_lod_clamp(1.0));
}
kernel void k(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]]) {
  o[0] = f(a, s);
}
