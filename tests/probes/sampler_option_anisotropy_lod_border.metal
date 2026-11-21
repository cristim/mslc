// EXPECT: valid
// max_anisotropy, lod_clamp and border_color fill the embedded sampler's anisotropy, lod_min/lod_max and border_color (the first option wins, as for the others); a sampler that sets none keeps Apple's defaults.
// REFLECT: "compare_function": "Never", "anisotropy": 8, "border_color": "OpaqueWhite", "lod_min": 0.5, "lod_max": 3
// REFLECT: "compare_function": "Never", "anisotropy": 1, "border_color": "TransparentBlack", "lod_min": 0, "lod_max": 65504
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> t [[texture(0)]]) {
  constexpr sampler a(max_anisotropy(8), max_anisotropy(2), lod_clamp(0.5f, 3), lod_clamp(1, 2), border_color::opaque_white, border_color::opaque_black, filter::linear);
  constexpr sampler b(filter::nearest);
  return t.sample(a, float2(0.5)) + t.sample(b, float2(0.5));
}
