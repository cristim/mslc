// EXPECT: error gather through a coord::pixel sampler is not lowered
// Apple accepts it; Vulkan has no gather on an unnormalized sampler.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]]) {
  constexpr sampler s(coord::pixel);
  return a.gather(s, float2(1));
}
