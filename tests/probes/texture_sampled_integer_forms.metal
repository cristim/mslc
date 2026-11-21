// EXPECT: valid
// A sampled integer texture is an image with an int or uint sampled type: sample, read, gather and the size queries return integer vectors; short and ushort are narrowed after the lookup.
// DISASM-MATCH: OpTypeImage %uint 2D 2 0 0 1 Unknown
// DISASM-MATCH: OpTypeImage %int 2D 2 0 0 1 Unknown
// DISASM-MATCH: OpTypeImage %int 3D 2 0 0 1 Unknown
// DISASM-MATCH: OpTypeImage %uint Cube 2 0 0 1 Unknown
// DISASM-MATCH: OpImageSampleExplicitLod %v4uint
// DISASM-MATCH: OpImageFetch %v4int
// DISASM-MATCH: OpImageGather %v4int
// DISASM-MATCH: OpSConvert %v4short
// DISASM-MATCH: OpUConvert %v4ushort
// DISASM-NOT: OpTypeImage %float
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint> u [[texture(0)]], texture2d<int> i [[texture(1)]], texture3d<short> s3 [[texture(2)]], texturecube<ushort> c [[texture(3)]],
    texture2d_array<int> a [[texture(4)]], sampler smp [[sampler(0)]], device int4* o [[buffer(0)]], uint2 t [[thread_position_in_grid]]) {
  o[0] = int4(u.sample(smp, float2(0.5)));
  o[1] = i.read(t);
  o[2] = int4(s3.read(uint3(t, 0)).x);
  o[3] = int4(c.sample(smp, float3(1, 0, 0)).x);
  o[4] = a.gather(smp, float2(0.5), 1u);
  o[5] = int4(u.get_width(), u.get_num_mip_levels(), 0, 0);
}
