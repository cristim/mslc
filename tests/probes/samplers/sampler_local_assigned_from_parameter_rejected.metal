// EXPECT: error cannot be assigned
// "sampler t; t = s;" is a run-time choice too.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]], constant int& c [[buffer(0)]]) {
  sampler u;
  u = s1;
  return t0.sample(u, float2(0.5));
}
