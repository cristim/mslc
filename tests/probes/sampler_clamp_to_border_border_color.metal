// EXPECT: valid
// clamp_to_border takes a border color; a clamp_to_zero axis with the default transparent black does too.
// REFLECT: "border_color": "OpaqueWhite"
// REFLECT: "border_color": "TransparentBlack"
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> t [[texture(0)]]) {
  constexpr sampler a(address::clamp_to_border, border_color::opaque_white);
  constexpr sampler b(address::clamp_to_zero, border_color::transparent_black);
  return t.sample(a, float2(0.5)) + t.sample(b, float2(0.5));
}
