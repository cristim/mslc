// EXPECT: error member helper "Lookup::look" names a file-scope sampler, pass the sampler to it as an argument
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct Lookup { float x; float4 look(texture2d<float> t) const { return t.sample(S, float2(x)); } };
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  Lookup l; l.x = 0.25; return l.look(t) + t.sample(S, float2(0.5));
}
fragment float4 g(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  Lookup l; l.x = 0.75; return l.look(t) + t.sample(S, float2(0.5));
}
