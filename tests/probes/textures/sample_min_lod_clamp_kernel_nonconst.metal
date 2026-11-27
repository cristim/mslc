// EXPECT: valid
// DISASM: OpImageSampleExplicitLod
// DISASM-NOT: OpImageSampleImplicitLod
// DISASM-NOT: MinLod
// Measured on Apple M1 Pro GPU (4-level 8x8 RGBA32F, nearest and linear mips): non-constant min_lod_clamp(m) in a kernel reads level 0.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]], constant float& m [[buffer(1)]]) {
  o[0] = a.sample(s, float2(0.25), min_lod_clamp(m));
}
