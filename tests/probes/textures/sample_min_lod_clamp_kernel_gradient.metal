// EXPECT: valid
// DISASM: OpCapability MinLod
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Grad\|MinLod
// DISASM-MATCH: OpImageSampleDrefExplicitLod %float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Grad\|MinLod
// Measured on Apple M1 Pro GPU (4-level 8x8 textures, nearest and linear mip filters):
// in a kernel function, gradient* with min_lod_clamp(m) clamps the gradient LOD to m, matching Grad|MinLod.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float> a [[texture(0)]], depth2d<float> d [[texture(1)]],
              sampler s [[sampler(0)]], device float4* o [[buffer(0)]]) {
  o[0] = a.sample(s, float2(0.25), gradient2d(float2(0.1), float2(0.1)), min_lod_clamp(1.0));
  o[1] = float4(d.sample_compare(s, float2(0.25), 0.25, gradient2d(float2(0.25, 0.0), float2(0.0, 0.25)), min_lod_clamp(2.0)));
}
