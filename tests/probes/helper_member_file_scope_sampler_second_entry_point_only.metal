// EXPECT: valid
// Only the second entry point reaches the member, so only it lists S; the first has no sampler.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "S" }
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct Lookup { float x; float4 look(texture2d<float> t) const { return t.sample(S, float2(x)); } };
fragment float4 f(texture2d<float> t [[texture(0)]]) { return t.read(uint2(0)); }
fragment float4 g(texture2d<float> t [[texture(0)]]) { Lookup l; l.x = 0.75; return l.look(t); }
