// EXPECT: valid
// DISASM: OpImageSampleExplicitLod
// DISASM-NOT: OpImageSampleImplicitLod
// DISASM-NOT: Bias
// Measured on Apple's GPU (RGBA32F 4x4 texture, nearest mips, one level per value): in a kernel a plain
// sample reads level 0 and bias(1.0), bias(2.0), bias(-1.0) read levels 1, 2, 0, so bias(b) is level b.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]]) {
  o[0] = a.sample(s, float2(0.25), bias(1.0));
}
