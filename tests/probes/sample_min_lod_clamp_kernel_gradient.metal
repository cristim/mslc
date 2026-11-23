// EXPECT: error min_lod_clamp with gradients in a kernel is not lowered
// It compiles to Grad|MinLod; Apple's result for it was not measured.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]]) {
  o[0] = a.sample(s, float2(0.25), gradient2d(float2(0.1), float2(0.1)), min_lod_clamp(1.0));
}
