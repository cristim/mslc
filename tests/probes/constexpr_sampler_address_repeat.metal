// EXPECT: valid
// address:: sets all three axes.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "s" }
// REFLECT: { "s_address": "Repeat", "t_address": "Repeat", "r_address": "Repeat", "mag_filter": "Nearest", "min_filter": "Nearest", "mip_filter": "None", "normalized_coordinates": true }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(address::repeat);
  return t.sample(s, float2(0.25));
}
