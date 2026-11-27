// EXPECT: valid
// DISASM: OpImageSampleDrefExplicitLod
// DISASM-NOT: OpImageSampleDrefImplicitLod
// DISASM-NOT: MinLod
// Measured on Apple M1 Pro GPU (4-level 8x8 Depth32Float): depth sample_compare with min_lod_clamp in a kernel reads level 0.
#include <metal_stdlib>
using namespace metal;
kernel void k(depth2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float* o [[buffer(0)]]) {
  o[0] = a.sample_compare(s, float2(0.25), 0.5, min_lod_clamp(1.0));
}
