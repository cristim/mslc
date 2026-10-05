// EXPECT: error the sample option gradient2d(...) is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.25), gradient2d(float2(0.0), float2(0.0)));
}
