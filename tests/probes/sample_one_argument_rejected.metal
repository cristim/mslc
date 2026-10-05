// EXPECT: error sample takes a sampler, a coordinate and optionally level(lod)
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(float2(0.25));
}
