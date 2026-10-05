// EXPECT: valid
// Apple takes sample(s, 0.5) and uses 0.5 for both components.
// DISASM-MATCH: OpCompositeConstruct %v2float %float_0_5 %float_0_5
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, 0.5);
}
