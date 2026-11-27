// EXPECT: error is not a border color
// Apple: no type named 'red' in 'metal::border_color'.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(border_color::red);
  return t.sample(s, float2(0.5));
}
