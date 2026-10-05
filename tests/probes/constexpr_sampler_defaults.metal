// EXPECT: valid
// A constexpr sampler with no options is nearest, clamp_to_edge, no mip filter,
// normalized coordinates: the state Apple's AIR records for it. It is a sampler variable
// at the next binding, and its state is in the reflection for indium to build the
// immutable sampler from (Iridium: EmbeddedSampler, air.cpp:844-935).
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "s" }
// REFLECT: { "s_address": "ClampToEdge", "t_address": "ClampToEdge", "r_address": "ClampToEdge", "mag_filter": "Nearest", "min_filter": "Nearest", "mip_filter": "None", "normalized_coordinates": true }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s;
  return t.sample(s, float2(0.25));
}
