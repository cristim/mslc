// EXPECT: valid
// A constant texel offset is the ConstOffset operand, with every option and in a kernel too.
// DISASM-MATCH: ConstOffset %[0-9A-Za-z_]+
// DISASM-MATCH: Bias\|ConstOffset
// DISASM-MATCH: Lod\|ConstOffset
// DISASM-MATCH: Grad\|ConstOffset
// DISASM-MATCH: ConstantComposite %v3int
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]], texture3d<float> c [[texture(1)]], texture2d_array<float> b [[texture(2)]], sampler s [[sampler(0)]]) {
  float4 r = a.sample(s, float2(0.25), int2(1, -1));
  r += a.sample(s, float2(0.25), bias(1.0), int2(2));
  r += a.sample(s, float2(0.25), level(1.0), int2(0, 1));
  r += a.sample(s, float2(0.25), gradient2d(float2(0.1), float2(0.2)), int2(1, 1));
  r += c.sample(s, float3(0.5), int3(1, 0, -1));
  return r + b.sample(s, float2(0.25), 1u, int2(1, 0));
}
kernel void k(texture2d<float> a [[texture(0)]], sampler s [[sampler(0)]], device float4* o [[buffer(0)]]) {
  o[0] = a.sample(s, float2(0.25), int2(1, 1));
}
