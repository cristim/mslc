// EXPECT: valid
// A typedef naming half is accepted for texturecube as well.
#include <metal_stdlib>
using namespace metal;
typedef half H;
typedef H Chained;
fragment float4 f(texturecube<Chained> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return float4(t.sample(s, float3(0.0, 0.0, 1.0)));
}
