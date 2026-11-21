// EXPECT: valid
// A free helper that calls an instance member that names the sampler.
// REFLECT: "embedded_sampler": 0, "name": "S"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct Lookup { float x; float4 look(texture2d<float> t) const { return t.sample(S, float2(x)); } };
float4 viaFree(texture2d<float> t, Lookup l) { return l.look(t); }
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  Lookup l; l.x = 0.25; return viaFree(t, l);
}
