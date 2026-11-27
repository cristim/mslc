// EXPECT: valid
// A constexpr sampler handed to a helper is still the entry point's embedded sampler.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "s" }
// REFLECT: "mag_filter": "Linear", "min_filter": "Linear", "mip_filter": "None", "normalized_coordinates": true
// REFLECT-NOT: "kind": "Sampler", "metal_index"
#include <metal_stdlib>
using namespace metal;
float4 look(texture2d<float> t, sampler s, float2 uv) { return t.sample(s, uv); }
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(filter::linear);
  return look(t, s, float2(0.25));
}
