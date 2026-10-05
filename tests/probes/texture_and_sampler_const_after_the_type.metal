// EXPECT: valid
// texture2d<float> const t and sampler const s are accepted by Apple.
// DISASM: OpTypeSampler
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> const t [[texture(0)]], sampler const s [[sampler(0)]]) {
  return t.sample(s, float2(0.25));
}
