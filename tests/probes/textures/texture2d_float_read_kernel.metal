// EXPECT: valid
// DISASM-MATCH: OpTypeImage %float 2D 2 0 0 1 Unknown
// DISASM-NOT: OpTypeImage %uint
// DISASM-NOT: NonWritable
// DISASM-NOT: NonReadable
// DISASM-NOT: StorageImageReadWithoutFormat
// DISASM-NOT: StorageImageWriteWithoutFormat
// DISASM-MATCH: OpImageFetch %v4float
// DISASM-NOT: OpImageRead
// DISASM-NOT: OpFConvert
// DISASM: OpImageQuerySizeLod %v2uint
// REFLECT: "metal_index": 5, "descriptor": { "set": 0, "binding": 1 }, "texture_access": "Read"
// A float access::read texture2d is a sampled image with a float sampled type, fetched from, as for uint.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::read> src [[texture(5)]], device float4* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  float4 a = src.read(uint2(i, 0u));
  float4 b = src.read(ushort2(1, 2));
  out[i] = a + b + float4(float(src.get_width()), float(src.get_height()), 0.0, 0.0);
}
