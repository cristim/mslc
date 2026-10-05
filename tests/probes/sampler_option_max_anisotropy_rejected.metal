// EXPECT: error the sampler option "max_anisotropy" is not lowered yet
// Apple accepts it; the reflection has no field for it.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(max_anisotropy(4));
  return t.sample(s, float2(0.25));
}
