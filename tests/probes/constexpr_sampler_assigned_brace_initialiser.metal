// EXPECT: valid
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "s" }
// REFLECT: { "s_address": "ClampToEdge", "t_address": "ClampToEdge", "r_address": "ClampToEdge", "mag_filter": "Linear", "min_filter": "Linear", "mip_filter": "None", "normalized_coordinates": true }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s = {filter::linear};
  return t.sample(s, float2(0.25));
}
