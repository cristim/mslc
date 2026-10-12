// EXPECT: error the third argument of sample has to be level(lod)
// Apple accepts gradient3d(); it is not lowered.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture3d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float3(1.0), gradient3d(float3(0.0), float3(0.0)));
}
