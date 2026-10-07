// EXPECT: valid
// A helper that names a file-scope sampler itself reserves it for the entry point that reaches it.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "S" }
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
float4 look(texture2d<float> t, float2 uv) { return t.sample(S, uv); }
float4 outer(texture2d<float> t, float2 uv) { return look(t, uv); }
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  return outer(t, float2(0.25));
}
