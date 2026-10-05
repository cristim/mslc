// EXPECT: valid
// s_address::repeat then address::clamp_to_edge repeats s and clamps t and r, as Apple.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "s" }
// REFLECT: { "s_address": "Repeat", "t_address": "ClampToEdge", "r_address": "ClampToEdge", "mag_filter": "Nearest", "min_filter": "Nearest", "mip_filter": "None", "normalized_coordinates": true }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(s_address::repeat, address::clamp_to_edge);
  return t.sample(s, float2(0.25));
}
