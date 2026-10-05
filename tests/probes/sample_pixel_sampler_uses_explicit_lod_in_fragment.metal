// EXPECT: valid
// Vulkan does not allow an implicit-lod lookup through a sampler with unnormalized
// coordinates, and Metal samples level 0 with coord::pixel, so a fragment lookup
// through one is the explicit-lod form at Lod 0.
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+ Lod %float_0(_[0-9])?[^_0-9a-zA-Z]
// DISASM-NO-MATCH: OpImageSampleImplicitLod
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]]) {
  constexpr sampler s(coord::pixel);
  return t.sample(s, float2(2.0));
}
