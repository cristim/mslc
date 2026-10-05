// EXPECT: valid
// s_address, t_address and r_address set one axis each.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "s" }
// REFLECT: { "s_address": "Repeat", "t_address": "MirrorRepeat", "r_address": "ClampToZero", "mag_filter": "Nearest", "min_filter": "Nearest", "mip_filter": "None", "normalized_coordinates": true }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(s_address::repeat, t_address::mirrored_repeat, r_address::clamp_to_zero);
  return t.sample(s, float2(0.25));
}
