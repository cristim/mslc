// EXPECT: valid
// DISASM: OpImageSampleExplicitLod
// DISASM-NOT: OpImageSampleImplicitLod
// DISASM-NOT: MinLod
// Measured on Apple's GPU (RGBA32F 4x4 texture, nearest mips, one level per value): in a kernel
// min_lod_clamp(1.0) alone reads level 0, as if ignored, the same as a plain sample, so mslc drops it.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]]) {
  o[0] = a.sample(s, float2(0.25), min_lod_clamp(1.0));
}
