// EXPECT: valid
// A float access::read texture2d is a storage image with a float sampled type, as for uint.
// DISASM-MATCH: OpTypeImage %float 2D 0 0 0 2 Unknown
// DISASM-NOT: OpTypeImage %uint
// DISASM: NonWritable
// DISASM-NOT: NonReadable
// DISASM: OpCapability StorageImageReadWithoutFormat
// DISASM-NOT: StorageImageWriteWithoutFormat
// DISASM-MATCH: OpImageRead %v4float
// DISASM-NOT: OpImageFetch
// DISASM-NOT: OpFConvert
// DISASM: OpImageQuerySize %v2uint
// DISASM-NOT: OpImageQuerySizeLod
// REFLECT: "metal_index": 5, "descriptor": { "set": 0, "binding": 1 }, "texture_access": "Read"
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::read> src [[texture(5)]], device float4* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  float4 a = src.read(uint2(i, 0u));
  float4 b = src.read(ushort2(1, 2));
  out[i] = a + b + float4(float(src.get_width()), float(src.get_height()), 0.0, 0.0);
}
