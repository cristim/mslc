// EXPECT: valid
// The direction is passed whole, as a three-component vector, with no face selection of
// our own: the hardware picks the face.
// DISASM-MATCH: OpImageSampleImplicitLod %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+[^_0-9a-zA-Z]
// DISASM-MATCH: OpCompositeConstruct %v3float %float_0_25 %float_0_5 %float_0_75
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]], sampler s [[sampler(0)]]) {
  return t.sample(s, float3(0.25, 0.5, 0.75));
}
