// EXPECT: valid
// A static member that names S, reached by a fragment and a vertex entry point: each stage's copy
// names its own stage's sampler.
// REFLECT: "embedded_sampler": 0, "name": "S"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct Lookup { float x; static float4 look(texture2d<float> t) { return t.sample(S, float2(0.25)); } };
struct V { float4 position [[position]]; };
fragment float4 f(texture2d<float> t [[texture(0)]]) { return Lookup::look(t); }
vertex V v(texture2d<float> t [[texture(0)]]) { V o; o.position = Lookup::look(t); return o; }
