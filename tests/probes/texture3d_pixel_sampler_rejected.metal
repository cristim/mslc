// EXPECT: error a texture3d cannot be sampled with a coord::pixel sampler
// Vulkan allows unnormalized coordinates on 1D and 2D image views only.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture3d<float> t [[texture(0)]]) {
  constexpr sampler px(coord::pixel);
  return t.sample(px, float3(0.5));
}
