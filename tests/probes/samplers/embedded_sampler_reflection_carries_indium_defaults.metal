// EXPECT: valid
// indium's EmbeddedSampler has more fields than mslc has options for. The ones no option
// sets hold Apple's defaults for a plain constexpr sampler (its AIR sampler state word):
// compare Never, anisotropy 1, border TransparentBlack, lod 0 to 65504. A consumer that
// zero-filled lod_max would restrict the sampler to mip level 0.
// REFLECT: "compare_function": "Never", "anisotropy": 1, "border_color": "TransparentBlack", "lod_min": 0, "lod_max": 65504 }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(filter::linear);
  return t.sample(s, float2(0.25));
}
