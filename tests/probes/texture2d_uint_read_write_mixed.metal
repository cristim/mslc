// EXPECT: valid
// DISASM-MATCH: OpTypeImage %uint 2D 2 0 0 1 Unknown
// DISASM-MATCH: OpTypeImage %uint 2D 0 0 0 2 Unknown
// DISASM-NOT: StorageImageReadWithoutFormat
// DISASM: OpCapability StorageImageWriteWithoutFormat
// DISASM: NonReadable
// DISASM-MATCH: OpImageFetch %v4uint
// DISASM: OpImageWrite
// REFLECT: "texture_access": "Read"
// REFLECT: "texture_access": "Write"
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::read> a [[texture(0)]], texture2d<uint, access::write> b [[texture(1)]], uint i [[thread_position_in_grid]]) {
  b.write(a.read(uint2(i, 0u)).x, uint2(i, 0u));
}
