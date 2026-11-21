// EXPECT: error is not a sampler
// Apple: use of undeclared identifier 'S'. The member cannot see a sampler declared below it.
#include <metal_stdlib>
using namespace metal;
struct Lookup { float x; float4 look(texture2d<float> t) const { return t.sample(S, float2(x)); } };
constexpr sampler S(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  Lookup l; l.x = 0.25; return l.look(t);
}
