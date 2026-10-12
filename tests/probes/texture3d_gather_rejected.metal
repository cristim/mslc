// EXPECT: error the texture method "gather" is not lowered yet
// Apple has no texture3d::gather either; this pins that mslc does not accept it.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture3d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.gather(s, float3(0.5));
}
