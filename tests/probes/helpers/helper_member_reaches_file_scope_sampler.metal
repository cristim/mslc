// EXPECT: valid
// A member that calls free helpers, the last of which names the sampler.
// REFLECT: "embedded_sampler": 0, "name": "S"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
float4 sampleGlobal(texture2d<float> t, float2 uv) { return t.sample(S, uv); }
float4 outer(texture2d<float> t, float2 uv) { return sampleGlobal(t, uv); }
struct Lookup { float x; float4 look(texture2d<float> t) const { return outer(t, float2(x)); } };
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  Lookup l; l.x = 0.25; return l.look(t);
}
