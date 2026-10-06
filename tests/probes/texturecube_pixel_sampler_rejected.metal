// EXPECT: error a texturecube cannot be sampled with a coord::pixel sampler
// Apple accepts it; Vulkan allows unnormalized coordinates on 1D and 2D image views only,
// so the module would be invalid at draw time. A sampler parameter is not checked.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> c [[texture(0)]]) {
  constexpr sampler cs(coord::pixel);
  return c.sample(cs, float3(0.5));
}
