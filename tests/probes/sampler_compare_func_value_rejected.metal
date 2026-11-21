// EXPECT: error is not a comparison function
// Apple: no member named 'bogus' in compare_func.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(compare_func::bogus);
  return t.sample(s, float2(0.5));
}
