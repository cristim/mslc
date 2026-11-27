// EXPECT: valid
// DISASM: OpImageSampleExplicitLod
// DISASM: OpImageSampleDrefExplicitLod
// DISASM-NOT: OpImageSampleImplicitLod
// DISASM-NOT: OpImageSampleDrefImplicitLod
// DISASM-NOT: Bias
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Lod %float_1(_[0-9]+)?[^_0-9A-Za-z]
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Lod %float_0_5[0-9_]*[^_0-9A-Za-z]
// Measured on Apple M1 Pro GPU (4-level 8x8 textures, nearest and linear mip filters):
// in a vertex function plain sample reads level 0 and bias(b) reads level b (clamped at 0
// and sampler lod bounds), matching OpImageSampleExplicitLod with Lod b.
#include <metal_stdlib>
using namespace metal;
float4 sampleHelper(texture2d<float> a, sampler s, float b) {
  return a.sample(s, float2(0.25), bias(b));
}
struct VOut { float4 pos [[position]]; float4 c [[user(locn0)]]; };
vertex VOut v(texture2d<float> a [[texture(0)]], texture2d_array<float> arr [[texture(1)]],
              texture3d<float> t3 [[texture(2)]], texturecube<float> tc [[texture(3)]],
              depth2d<float> td [[texture(4)]], sampler s [[sampler(0)]],
              constant float& b [[buffer(0)]]) {
  VOut o;
  o.pos = a.sample(s, float2(0.25), bias(1.0));
  o.c = a.sample(s, float2(0.25), bias(0.5));
  o.c += a.sample(s, float2(0.25), bias(-1.0));
  o.c += sampleHelper(a, s, b);
  o.c += arr.sample(s, float2(0.25), 1u, bias(2.0));
  o.c += t3.sample(s, float3(0.25), bias(1.0));
  o.c += tc.sample(s, float3(1.0, 0.0, 0.0), bias(2.0));
  o.c += float4(td.sample_compare(s, float2(0.25), 0.25, bias(2.0)));
  return o;
}
