// EXPECT: error sample takes a sampler, a coordinate, an array index and optionally level(lod)
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d_array<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.5));
}
