// EXPECT: error is not lowered yet
// A typedef that names a vector type is still not a sampled type.
#include <metal_stdlib>
using namespace metal;
typedef float4 Vec;
fragment float4 f(texture2d<Vec> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.0, 0.0));
}
