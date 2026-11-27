// EXPECT: error "filter::bicubic" is accepted by Apple but not lowered: Vulkan has bicubic filtering only as VK_EXT_filter_cubic
// Apple's metal accepts filter::bicubic. Vulkan core has no bicubic filter and the reflection's
// sampler has no field for it, so the option is refused rather than dropped.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(filter::bicubic);
  return t.sample(s, float2(0.5));
}
