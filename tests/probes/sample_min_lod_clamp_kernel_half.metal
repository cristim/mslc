// EXPECT: valid
// DISASM: OpImageSampleExplicitLod
// DISASM-NOT: OpImageSampleImplicitLod
// DISASM-NOT: MinLod
// Measured on Apple M1 Pro GPU (4-level 8x8 RGBA16Float): min_lod_clamp(1.0) on texture2d<half> in a kernel reads level 0.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<half> a [[texture(0)]], sampler s [[sampler(0)]], device half4* o [[buffer(0)]]) {
  o[0] = a.sample(s, float2(0.25), min_lod_clamp(1.0));
}
