// EXPECT: error member helper "Lookup::inner" names a file-scope sampler, pass the sampler to it as an argument
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
