// EXPECT: error "bogus" is not a sampler option
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(bogus::linear);
  return t.sample(s, float2(0.25));
}
