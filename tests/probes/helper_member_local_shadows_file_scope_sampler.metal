// EXPECT: valid
// A local of the same name hides the file-scope sampler inside the member, so none is reserved.
// REFLECT-NOT: "name": "S"
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct Lookup { float x; float4 look(texture2d<float> t, sampler s) const { float S = x; return t.sample(s, float2(S)); } };
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  Lookup l; l.x = 0.25; return l.look(t, s);
}
