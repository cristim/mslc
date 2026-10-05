// EXPECT: valid
// Vulkan requires Lod 0 through an unnormalized sampler, and Apple accepts level(2.0)
// there, so the operand is 0 whatever level() says.
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Lod %float_0(_[0-9])?[^_0-9a-zA-Z]
// DISASM-NO-MATCH: Lod %float_2
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(coord::pixel);
  return t.sample(s, float2(2.25), level(2.0));
}
