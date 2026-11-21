// EXPECT: error bias and min_lod_clamp are lowered in a fragment function only
// Apple accepts bias in a kernel; a kernel has no derivatives to bias, so mslc refuses instead of guessing the level.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]]) {
  o[0] = a.sample(s, float2(0.25), bias(1.0));
}
