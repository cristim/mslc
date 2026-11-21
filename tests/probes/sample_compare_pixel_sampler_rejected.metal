// EXPECT: error sample_compare through a coord::pixel sampler is not lowered
// Apple accepts it; Vulkan allows no depth comparison on an unnormalized sampler.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(depth2d<float> t [[texture(0)]]) {
  constexpr sampler s(coord::pixel);
  return float4(t.sample_compare(s, float2(1), 0.5));
}
