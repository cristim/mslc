// EXPECT: valid
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Lod\|ConstOffset
// DISASM-NOT: OpImageSampleImplicitLod
// DISASM-NOT: MinLod
// Measured on Apple M1 Pro GPU (4-level 8x8 RGBA32F): min_lod_clamp(1.0) with int2 offset in a kernel reads level 0 at the offset texel.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]]) {
  o[0] = a.sample(s, float2(0.25), min_lod_clamp(1.0), int2(1, 0));
}
