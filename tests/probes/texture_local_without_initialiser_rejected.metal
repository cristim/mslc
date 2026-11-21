// EXPECT: error needs an initialiser that names a texture
// Apple accepts "texture2d<float> b; b = t0;" and a default-constructed texture.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t0 [[texture(0)]], texture2d<float> t1 [[texture(1)]], sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]], constant int& c [[buffer(0)]]) {
  texture2d<float> b;
  return float4(0.0);
}
