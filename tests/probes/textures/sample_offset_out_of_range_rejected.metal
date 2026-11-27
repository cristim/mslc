// EXPECT: error the texel offset 8 is outside -8 to 7
// Apple does not diagnose it; Vulkan guarantees -8 to 7 only, so mslc refuses a literal beyond it.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]]) {
  return a.sample(s, float2(0.5), int2(8, 0));
}
