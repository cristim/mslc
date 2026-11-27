// EXPECT: error is not a texture parameter
#include <metal_stdlib>
using namespace metal;
fragment float4 f(sampler s [[sampler(0)]]) {
  return s.sample(s, float2(0.25));
}
