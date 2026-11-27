// EXPECT: valid
// const on a texture or a sampler parameter is accepted.
// DISASM: OpTypeSampler
#include <metal_stdlib>
using namespace metal;
fragment float4 f(const texture2d<float> t [[texture(0)]], const sampler s [[sampler(0)]]) {
  return t.sample(s, float2(0.25));
}
