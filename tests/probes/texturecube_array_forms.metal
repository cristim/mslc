// EXPECT: valid
// A texturecube_array is a Dim Cube arrayed image (SampledCubeArray/ImageCubeArray): sample takes a float3 and a layer, gather and gradientcube work, a storage access takes face and array index as one 6 * index + face layer-face coordinate, get_array_size counts cubes.
// DISASM: OpCapability ImageCubeArray
// DISASM-MATCH: OpTypeImage %float Cube 2 1 0 1 Unknown
// DISASM-MATCH: OpTypeImage %float Cube 0 1 0 2 Unknown
// DISASM-MATCH: OpIMul %uint %[0-9A-Za-z_]+ %uint_6
// DISASM-MATCH: OpImageQuerySizeLod %v3uint
// DISASM: OpImageGather
// DISASM: Grad
// DISASM: OpImageQueryLevels
#include <metal_stdlib>
using namespace metal;
fragment float4 hit(texturecube_array<float> a [[texture(0)]], texturecube_array<float, access::read_write> b [[texture(1)]], sampler s [[sampler(0)]]) {
  float4 r = a.sample(s, float3(1, 0, 0), 2u) + a.gather(s, float3(0, 1, 0), 1u, component::y);
  r += a.sample(s, float3(1, 0, 0), 1u, gradientcube(float3(0.1), float3(0.2)));
  r += b.read(uint2(1), 3u, 2u);
  b.write(r, uint2(1), 3u, 2u);
  return r + float4(a.get_width(), a.get_height(), a.get_array_size(), a.get_num_mip_levels());
}
