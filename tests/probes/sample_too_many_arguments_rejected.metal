// EXPECT: error sample takes a sampler, a coordinate and optionally an option
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.25), level(1.0), min_lod_clamp(1.0), int2(1, 1), int2(1, 1));
}
