// EXPECT: error max_anisotropy takes an integer from 1 to 16
// Apple accepts any value; mslc cannot know what it records for one outside the range.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(max_anisotropy(32));
  return t.sample(s, float2(0.5));
}
