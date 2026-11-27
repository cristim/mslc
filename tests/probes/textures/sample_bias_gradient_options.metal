// EXPECT: valid
// bias is the Bias operand of an implicit-lod sample; gradient2d, gradient3d and gradientcube are Grad operands of an explicit-lod one; min_lod_clamp needs the MinLod capability.
// DISASM: OpCapability MinLod
// DISASM-MATCH: OpImageSampleImplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Bias %[0-9A-Za-z_]+
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Grad %[0-9A-Za-z_]+ %[0-9A-Za-z_]+
// DISASM-MATCH: OpImageSampleImplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ MinLod %[0-9A-Za-z_]+
// DISASM-MATCH: OpImageSampleImplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Bias\|MinLod %float_0_5[0-9_]* %float_2[0-9_]*
// DISASM-MATCH: OpImageSampleExplicitLod %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Grad\|MinLod %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %float_2[0-9_]*
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]], texture2d_array<half> b [[texture(1)]], texture3d<float> c [[texture(2)]], texturecube<float> d [[texture(3)]], sampler s [[sampler(0)]]) {
  float4 r = a.sample(s, float2(0.25), bias(1.5));
  r += float4(b.sample(s, float2(0.25), 2u, bias(-1.0)));
  r += a.sample(s, float2(0.25), gradient2d(float2(0.1), float2(0.2)));
  r += float4(b.sample(s, float2(0.25), 1u, gradient2d(float2(0.1), float2(0.2))));
  r += c.sample(s, float3(0.5), gradient3d(float3(0.1), float3(0.2)));
  r += d.sample(s, float3(1, 0, 0), gradientcube(float3(0.1), float3(0.2)));
  r += a.sample(s, float2(0.25), min_lod_clamp(1.0));
  r += a.sample(s, float2(0.25), bias(0.5), min_lod_clamp(2.0));
  r += a.sample(s, float2(0.25), gradient2d(float2(0.1), float2(0.2)), min_lod_clamp(2.0));
  return r;
}
