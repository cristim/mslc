// EXPECT: error cannot be assigned
// Apple accepts b = t1, which picks the descriptor at run time; mslc binds the name once.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]], constant int& c [[buffer(0)]]) {
  texture2d<float> b = t0;
  b = t1;
  return b.sample(s0, float2(0.5));
}
