// EXPECT: valid
// DISASM: OpImageSampleImplicitLod
// REFLECT-NOT: embedded_sampler": 0
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct Lookup { float x; float4 look(texture2d<float> t, sampler S) const { return t.sample(S, float2(x)); } };
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  Lookup l; l.x = 0.25; return l.look(t, s);
}
