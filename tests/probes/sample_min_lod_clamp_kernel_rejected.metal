// EXPECT: error min_lod_clamp, and bias outside a kernel
// Apple accepts min_lod_clamp in a kernel; on its GPU min_lod_clamp(1.0) read level 0, as if ignored, which
// Vulkan's MinLod does not do, so mslc refuses it.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]]) {
  o[0] = a.sample(s, float2(0.25), min_lod_clamp(1.0));
}
