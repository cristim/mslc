// EXPECT: error expected a sampler option after ','
// Apple: expected expression.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(lod_clamp(1, 2),);
  return t.sample(s, float2(0.5));
}
