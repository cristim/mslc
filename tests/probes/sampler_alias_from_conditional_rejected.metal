// EXPECT: error has to be initialised from the name of a texture or sampler
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]], constant int& c [[buffer(0)]]) {
  sampler u = c ? s0 : s1;
  return t0.sample(u, float2(0.5));
}
