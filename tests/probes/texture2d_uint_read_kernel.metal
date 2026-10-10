// EXPECT: valid
// DISASM-MATCH: OpTypeImage %uint 2D 0 0 0 2 Unknown
// DISASM: NonWritable
// DISASM-NOT: NonReadable
// DISASM: OpCapability StorageImageReadWithoutFormat
// DISASM-NOT: StorageImageWriteWithoutFormat
// DISASM-MATCH: OpImageRead %v4uint
// DISASM-NOT: OpImageFetch
// DISASM: OpImageQuerySize %v2uint
// DISASM-NOT: OpImageQuerySizeLod
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
