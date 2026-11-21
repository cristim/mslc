// EXPECT: error an array of references, pointers or resources is not lowered yet
// Apple accepts an array of textures indexed at run time.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]], constant int& c [[buffer(0)]]) {
  texture2d<float> arr[2] = { t0, t1 };
  return arr[c].sample(s0, float2(0.5));
}
