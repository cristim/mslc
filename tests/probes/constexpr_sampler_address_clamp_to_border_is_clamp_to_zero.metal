// EXPECT: valid
// With no border_color option the border is transparent black, which Apple's AIR records
// as the same mode as clamp_to_zero (its sampler state word has address 0 for both).
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "s" }
// REFLECT: { "s_address": "ClampToZero", "t_address": "ClampToZero", "r_address": "ClampToZero", "mag_filter": "Nearest", "min_filter": "Nearest", "mip_filter": "None", "normalized_coordinates": true, "compare_function": "Never", "anisotropy": 1, "border_color": "TransparentBlack", "lod_min": 0, "lod_max": 65504 }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(address::clamp_to_border);
  return t.sample(s, float2(0.25));
}
