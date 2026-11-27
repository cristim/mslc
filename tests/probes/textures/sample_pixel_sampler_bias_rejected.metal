// EXPECT: error a coord::pixel sampler takes level(lod) or no option only
// Apple accepts it; Vulkan fixes Lod 0 on an unnormalized sampler.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]]) {
  constexpr sampler s(coord::pixel);
  return a.sample(s, float2(1), bias(1.0));
}
