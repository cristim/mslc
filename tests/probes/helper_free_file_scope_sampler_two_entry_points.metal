// EXPECT: valid
// A free helper that names S, reached by two fragment entry points: one copy each.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "S" }
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 2 }, "embedded_sampler": 0, "name": "S" }
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
float4 look(texture2d<float> t) { return t.sample(S, float2(0.25)); }
fragment float4 a(texture2d<float> t [[texture(0)]]) { return look(t); }
fragment float4 b(texture2d<float> u [[texture(1)]], texture2d<float> t [[texture(0)]]) { return look(t); }
