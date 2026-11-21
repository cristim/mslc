// EXPECT: valid
// A helper that reaches S only through a local alias is still one that names S: one copy per entry
// point, each with its own sampler, as for a helper that names S directly (#167).
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "S" }
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 2 }, "embedded_sampler": 0, "name": "S" }
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
float4 look(texture2d<float> t) { sampler u = S; return t.sample(u, float2(0.25)); }
fragment float4 a(texture2d<float> t [[texture(0)]]) { return look(t); }
fragment float4 b(texture2d<float> u [[texture(1)]], texture2d<float> t [[texture(0)]]) { return look(t); }
