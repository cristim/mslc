// EXPECT: valid
// The sampler is named two members down, by an implicit-this call.
// REFLECT: "embedded_sampler": 0, "name": "S"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct Lookup {
  float x;
  float4 look(texture2d<float> t) const { return inner(t); }
  float4 inner(texture2d<float> t) const { return t.sample(S, float2(x)); }
};
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  Lookup l; l.x = 0.25; return l.look(t);
}
