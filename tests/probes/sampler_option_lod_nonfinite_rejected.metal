// EXPECT: error "lod_clamp" takes a finite float
// Apple accepts 1e40 (it becomes infinity in a float); the reflection would carry a non-JSON number.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(lod_clamp(0, 1e40));
  return t.sample(s, float2(0.5));
}
