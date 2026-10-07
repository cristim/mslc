// EXPECT: valid
// DISASM: OpImageSampleImplicitLod
// DISASM: OpImageSampleExplicitLod
#include <metal_stdlib>
using namespace metal;
struct V { float4 position [[position]]; };
struct Lookup { float2 uv; float4 look(texture2d<float> t, sampler s) const { return t.sample(s, uv); } };
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  Lookup l; l.uv = float2(0.25); return l.look(t, s);
}
vertex V v(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  Lookup l; l.uv = float2(0.25); V o; o.position = l.look(t, s); return o;
}
