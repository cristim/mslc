// EXPECT: error "filter::cubic" is not a sampler filter
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(filter::cubic);
  return t.sample(s, float2(0.25));
}
