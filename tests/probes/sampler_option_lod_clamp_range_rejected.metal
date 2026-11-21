// EXPECT: error lod_clamp takes a lower bound of at least 0
// Apple accepts an inverted or negative range; mslc cannot know what it records for one.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(lod_clamp(3.0, 2.0));
  return t.sample(s, float2(0.5));
}
