// EXPECT: error the sampler option "border_color" is not lowered yet
// Apple accepts it; the reflection has no field for it.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(border_color::opaque_white);
  return t.sample(s, float2(0.25));
}
