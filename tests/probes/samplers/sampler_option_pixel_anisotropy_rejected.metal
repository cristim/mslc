// EXPECT: error with coord::pixel, Vulkan needs anisotropy off and no lod range
// Apple accepts it; a Vulkan sampler with unnormalized coordinates cannot carry either.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(coord::pixel, max_anisotropy(4));
  return t.sample(s, float2(1));
}
