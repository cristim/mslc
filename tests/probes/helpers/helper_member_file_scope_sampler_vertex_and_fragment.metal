// EXPECT: valid
// One instance member reached by a vertex and a fragment entry point: a copy for each stage, each
// naming the sampler of its own stage's descriptor set.
// REFLECT: "stage": "vertex"
// REFLECT: "embedded_sampler": 0, "name": "S"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct Lookup { float x; float4 look(texture2d<float> t) const { return t.sample(S, float2(x), level(0.0)); } };
struct V { float4 position [[position]]; };
vertex V v(texture2d<float> t [[texture(0)]]) { V o; Lookup l; l.x = 0.5; o.position = l.look(t); return o; }
fragment float4 f(texture2d<float> t [[texture(0)]]) { Lookup l; l.x = 0.25; return l.look(t); }
