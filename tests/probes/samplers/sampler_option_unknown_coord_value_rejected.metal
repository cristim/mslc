// EXPECT: error "coord::absolute" is not a coordinate mode
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(coord::absolute);
  return t.sample(s, float2(0.25));
}
