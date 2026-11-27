// EXPECT: error a texture2d_array cannot be sampled with a coord::pixel sampler
// Vulkan allows unnormalized coordinates on non-arrayed 1D and 2D image views only.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d_array<float> t [[texture(0)]]) {
  constexpr sampler px(coord::pixel);
  return t.sample(px, float2(0.5), 0u);
}
