// EXPECT: valid
// A texturecube local is the cube it names: sampled with a float3 direction.
// DISASM: OpTypeImage %float Cube 2 0 0 1 Unknown
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  texturecube<float> x = t;
  return x.sample(s, float3(0.0, 0.0, 1.0));
}
