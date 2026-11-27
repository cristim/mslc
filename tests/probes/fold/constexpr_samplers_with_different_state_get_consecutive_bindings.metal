// EXPECT: valid
// Distinct states are distinct samplers at consecutive bindings after the textures, in
// declaration order.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "a" }
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 2 }, "embedded_sampler": 1, "name": "b" }
// REFLECT: { "s_address": "ClampToEdge", "t_address": "ClampToEdge", "r_address": "ClampToEdge", "mag_filter": "Linear", "min_filter": "Linear", "mip_filter": "None", "normalized_coordinates": true, "compare_function": "Never", "anisotropy": 1, "border_color": "TransparentBlack", "lod_min": 0, "lod_max": 65504 }
// REFLECT: { "s_address": "ClampToEdge", "t_address": "ClampToEdge", "r_address": "ClampToEdge", "mag_filter": "Nearest", "min_filter": "Nearest", "mip_filter": "None", "normalized_coordinates": true, "compare_function": "Never", "anisotropy": 1, "border_color": "TransparentBlack", "lod_min": 0, "lod_max": 65504 }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler a(filter::linear);
  constexpr sampler b(filter::nearest);
  return t.sample(a, float2(0.25)) + t.sample(b, float2(0.25));
}
