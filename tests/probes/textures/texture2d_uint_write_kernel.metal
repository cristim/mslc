// EXPECT: valid
// DISASM-MATCH: OpTypeImage %uint 2D 0 0 0 2 Unknown
// DISASM: NonReadable
// DISASM: OpCapability StorageImageWriteWithoutFormat
// DISASM-NOT: StorageImageReadWithoutFormat
// DISASM: OpImageWrite
// DISASM: OpCompositeConstruct %v4uint
// DISASM: OpImageQuerySize %v2uint
// DISASM-NOT: OpImageQuerySizeLod
// DISASM-NOT: OpConvertUToF
// REFLECT: "metal_index": 5, "descriptor": { "set": 0, "binding": 0 }, "texture_access": "Write"
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::write> dst [[texture(5)]], uint i [[thread_position_in_grid]]) {
  uint width = dst.get_width();
  uint value = i == 0u ? 0u : i == 1u ? 1u : i == 2u ? 16777217u : i == 3u ? 0x80000001u : 0xffffffffu;
  dst.write(value, uint2(i % width, i / width));
}
