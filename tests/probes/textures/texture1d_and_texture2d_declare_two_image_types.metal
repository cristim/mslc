// EXPECT: valid
// DISASM: OpTypeImage %float 1D 2 0 0 1 Unknown
// DISASM: OpTypeImage %float 2D 2 0 0 1 Unknown
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture1d<float> a [[texture(0)]], texture2d<float> b [[texture(1)]], sampler s [[sampler(0)]]) {
  return a.sample(s, 0.5f) + b.sample(s, float2(0.5));
}
