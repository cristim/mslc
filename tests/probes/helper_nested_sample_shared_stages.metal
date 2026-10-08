// EXPECT: valid
// DISASM: OpImageSampleImplicitLod
// DISASM: OpImageSampleExplicitLod
#include <metal_stdlib>
using namespace metal;
struct V { float4 position [[position]]; };
float4 inner(texture2d<float> t, sampler s) { return t.sample(s, float2(0.25)); }
float4 outer(texture2d<float> t, sampler s) { return inner(t, s); }
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) { return outer(t, s); }
vertex V v(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  V o; o.position = outer(t, s); return o;
}
