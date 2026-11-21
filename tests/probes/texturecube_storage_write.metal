// EXPECT: valid
// A cube storage image keeps Dim Cube (the host binds a cube view); read and write take (x, y, face) and the size is two wide.
// DISASM-MATCH: OpTypeImage %float Cube 0 0 0 2 Unknown
// DISASM-NOT: Arrayed
// DISASM-NOT: OpTypeImage %float 2D 0 1 0 2
// DISASM-NOT: OpTypeImage %half
// DISASM: OpCapability StorageImageWriteWithoutFormat
// DISASM: NonReadable
// DISASM-MATCH: OpImageWrite %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %[0-9A-Za-z_]+
// DISASM: OpImageQuerySize %v2uint
// REFLECT: "texture_access": "Write"
#include <metal_stdlib>
using namespace metal;
kernel void hit(texturecube<half, access::write> m [[texture(0)]], texturecube<float, access::read_write> rw [[texture(1)]], device uint* out [[buffer(0)]], uint3 g [[thread_position_in_grid]]) {
  m.write(half4(1), g.xy, g.z);
  rw.write(rw.read(g.xy, g.z), ushort2(g.xy), 3u);
  out[0] = m.get_width() + m.get_height() + rw.get_width();
}
