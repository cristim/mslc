// EXPECT: valid
// DISASM: OpImageSampleImplicitLod
// DISASM: OpImageSampleExplicitLod
#include <metal_stdlib>
using namespace metal;
struct V { float4 position [[position]]; };
float4 look(texture2d<float> t, sampler s) { return t.sample(s, float2(0.25)); }
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) { return look(t, s); }
vertex V v(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  V o; o.position = look(t, s); return o;
}
