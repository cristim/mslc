// EXPECT: valid
// The pixel restriction applies to the resolved state, as Apple's does: the later
// address::repeat is ignored because address::clamp_to_edge named the axes first.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "s" }
// REFLECT: { "s_address": "ClampToEdge", "t_address": "ClampToEdge", "r_address": "ClampToEdge", "mag_filter": "Nearest", "min_filter": "Nearest", "mip_filter": "None", "normalized_coordinates": false, "compare_function": "Never", "anisotropy": 1, "border_color": "TransparentBlack", "lod_min": 0, "lod_max": 65504 }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(address::clamp_to_edge, address::repeat, coord::pixel);
  return t.sample(s, float2(0.25));
}
