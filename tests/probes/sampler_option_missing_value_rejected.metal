// EXPECT: error expected a value after "filter::"
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(filter::);
  return t.sample(s, float2(0.25));
}
