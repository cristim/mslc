// EXPECT: valid
// A file-scope sampler handed to a helper is reserved for the entry point that names it.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "S" }
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
float4 look(texture2d<float> t, sampler s, float2 uv) { return t.sample(s, uv); }
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return look(t, S, float2(0.25));
}
