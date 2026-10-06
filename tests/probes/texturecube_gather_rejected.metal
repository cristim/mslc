// EXPECT: error the texturecube method "gather" is not lowered yet
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.gather(s, float3(0.5));
}
