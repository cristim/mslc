// EXPECT: valid
// Apple keeps the first option that names an attribute: address::repeat then
// address::clamp_to_edge is repeat (verified on the AIR's sampler state word).
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "s" }
// REFLECT: { "s_address": "Repeat", "t_address": "Repeat", "r_address": "Repeat", "mag_filter": "Nearest", "min_filter": "Nearest", "mip_filter": "None", "normalized_coordinates": true, "compare_function": "Never", "anisotropy": 1, "border_color": "TransparentBlack", "lod_min": 0, "lod_max": 65504 }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(address::repeat, address::clamp_to_edge);
  return t.sample(s, float2(0.25));
}
