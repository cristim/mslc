// EXPECT: error "address::wrap" is not a sampler address mode
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(address::wrap);
  return t.sample(s, float2(0.25));
}
