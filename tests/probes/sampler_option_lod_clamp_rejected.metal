// EXPECT: error the sampler option "lod_clamp" is not lowered yet
// Apple accepts it; the reflection has no field for it.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(lod_clamp(0.0, 1.0));
  return t.sample(s, float2(0.25));
}
