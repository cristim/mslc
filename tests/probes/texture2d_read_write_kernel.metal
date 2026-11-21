// EXPECT: valid
// access::read_write is a storage image both read and written: both capabilities, neither NonReadable nor NonWritable.
// DISASM-MATCH: OpTypeImage %float 2D 0 0 0 2 Unknown
// DISASM: OpCapability StorageImageReadWithoutFormat
// DISASM: OpCapability StorageImageWriteWithoutFormat
// DISASM-NOT: NonReadable
// DISASM-NOT: NonWritable
// DISASM-MATCH: OpImageRead %v4float
// DISASM: OpImageWrite
// DISASM: OpImageQuerySize %v2uint
// REFLECT: "metal_index": 1, "descriptor": { "set": 0, "binding": 0 }, "texture_access": "ReadWrite"
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<float, access::read_write> acc [[texture(1)]], uint2 t [[thread_position_in_grid]]) {
  if (t.x >= acc.get_width() || t.y >= acc.get_height()) return;
  acc.write(acc.read(t) * 0.5, t);
}
