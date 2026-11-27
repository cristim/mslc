// EXPECT: valid
// const on a texture or sampler parameter is accepted by Apple and changes nothing.
#include <metal_stdlib>
using namespace metal;
float4 look(const texture2d<float> t, const sampler s, float2 uv) { return t.sample(s, uv); }
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return look(t, s, float2(0.25));
}
