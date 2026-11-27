// EXPECT: valid
// DISASM-MATCH: OpTypeImage %uint 2D 2 0 0 1 Unknown
// DISASM-NOT: NonWritable
// DISASM-NOT: NonReadable
// DISASM-NOT: StorageImageReadWithoutFormat
// DISASM-NOT: StorageImageWriteWithoutFormat
// DISASM-MATCH: OpImageFetch %v4uint
// DISASM-NOT: OpImageRead
// DISASM: OpImageQuerySizeLod %v2uint
// REFLECT: "metal_index": 5, "descriptor": { "set": 0, "binding": 1 }, "texture_access": "Read"
#include <metal_stdlib>
using namespace metal;
kernel void hit(const metal::texture2d<unsigned int, metal::access::read> src [[texture(5)]], device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint read = src.get_width();
  uint4 a = src.read(uint2(i, 0u));
  uint4 b = src.read(ushort2(1, 2));
  src.read(uint2(0));
  out[i] = a.x + b.w + read + src.get_height();
}
