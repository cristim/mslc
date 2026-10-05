// EXPECT: error "mip_filter::bilinear" is not a mip filter
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(mip_filter::bilinear);
  return t.sample(s, float2(0.25));
}
