// EXPECT: valid
// A program-scope constexpr sampler is an embedded sampler, as a local one is. Apple accepts it.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "S" }
// REFLECT: "mag_filter": "Linear", "min_filter": "Linear", "mip_filter": "None", "normalized_coordinates": true
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(coord::normalized, address::clamp_to_edge, filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return t.sample(S, float2(0.25));
}
