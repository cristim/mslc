// EXPECT: error a texture3d takes gradient3d(...), not gradient2d(...)
// Apple: no matching member function for call to 'sample'.
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture3d<float> a [[texture(0)]], sampler s [[sampler(0)]]) {
  return a.sample(s, float3(0.5), gradient2d(float2(0), float2(0)));
}
