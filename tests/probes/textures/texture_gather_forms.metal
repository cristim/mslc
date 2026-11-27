// EXPECT: valid
// gather is OpImageGather on the sampled image: component is a constant, a literal offset is ConstOffset, a variable one the Offset operand with ImageGatherExtended.
// DISASM: OpCapability ImageGatherExtended
// DISASM-MATCH: OpImageGather %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ ConstOffset %[0-9A-Za-z_]+
// DISASM-MATCH: OpImageGather %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ Offset %[0-9A-Za-z_]+
// DISASM: OpFConvert %v4half
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texture2d<float> a [[texture(0)]], texture2d_array<half> b [[texture(1)]], texturecube<float> c [[texture(2)]], sampler s [[sampler(0)]], constant int2& q [[buffer(0)]]) {
  float4 r = a.gather(s, float2(0.5), int2(1, -1), component::y);
  r += float4(b.gather(s, float2(0.5), 2u, int2(0, 1)));
  r += c.gather(s, float3(1, 0, 0), component::z);
  r += a.gather(s, float2(0.25), q);
  return r + a.gather(s, float2(0.25));
}
