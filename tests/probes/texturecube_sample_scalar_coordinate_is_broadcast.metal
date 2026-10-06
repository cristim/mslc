// EXPECT: valid
// Apple takes sample(s, 0.5) on a texturecube and uses 0.5 for all three components.
// DISASM-MATCH: OpCompositeConstruct %v3float %float_0_5 %float_0_5 %float_0_5
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, 0.5);
}
