// EXPECT: error with coord::pixel the compare_func has to be never
// Apple: invalid values combination in sampler initialization.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(coord::pixel, compare_func::less);
  return t.sample(s, float2(1));
}
