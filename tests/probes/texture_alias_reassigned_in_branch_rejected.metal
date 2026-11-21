// EXPECT: error cannot be assigned
// Reassigned under a condition: the descriptor would depend on c, so it is refused, not resolved.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]], constant int& c [[buffer(0)]]) {
  auto b = t0;
  if (c) { b = t1; }
  return b.sample(s0, float2(0.5));
}
