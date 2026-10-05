// EXPECT: error the sampler option "compare_func" is not lowered yet
// Apple accepts it; the reflection has no field for it.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(compare_func::less);
  return t.sample(s, float2(0.25));
}
