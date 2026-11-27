// EXPECT: valid
// A depth texture is an image with Depth=1: sample and read return one float (the first channel), gather four, and the size queries work as for the color kinds.
// DISASM-MATCH: OpTypeImage %float 2D 1 0 0 1 Unknown
// DISASM-MATCH: OpTypeImage %float 2D 1 1 0 1 Unknown
// DISASM-MATCH: OpTypeImage %float Cube 1 0 0 1 Unknown
// DISASM-MATCH: OpTypeImage %float Cube 1 1 0 1 Unknown
// DISASM-MATCH: OpCompositeExtract %float %[0-9A-Za-z_]+ 0
// DISASM-MATCH: OpImageFetch %v4float
// DISASM: OpImageGather
// DISASM: OpImageQueryLevels
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(depth2d<float> a [[texture(0)]], depth2d_array<float> b [[texture(1)]], depthcube<float> c [[texture(2)]], depthcube_array<float> d [[texture(3)]], sampler s [[sampler(0)]]) {
  float x = a.sample(s, float2(0.5)) + a.sample(s, float2(0.5), level(1), int2(1, 0)) + a.read(uint2(1), 0);
  x += b.sample(s, float2(0.5), 1u) + c.sample(s, float3(1, 0, 0)) + d.sample(s, float3(1, 0, 0), 2u);
  float4 g = a.gather(s, float2(0.5), int2(1, 0)) + c.gather(s, float3(1, 0, 0));
  return g + float4(x, a.get_width(), a.get_num_mip_levels(), b.get_array_size() + d.get_array_size());
}
