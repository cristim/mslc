// EXPECT: error clamp_to_zero takes no border_color other than transparent_black
// Apple: invalid values combination (indium would apply the border color to the zero mode).
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(address::clamp_to_zero, border_color::opaque_white);
  return t.sample(s, float2(0.5));
}
