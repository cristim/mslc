// EXPECT: valid
// DISASM-MATCH: OpTypeImage %uint 2D 0 0 0 2 Unknown
// DISASM-NOT: NonReadable
// DISASM-NOT: NonWritable
// DISASM-MATCH: OpImageRead %v4uint
// DISASM: OpImageWrite
// REFLECT: "texture_access": "ReadWrite"
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::read_write> a [[texture(0)]], uint2 t [[thread_position_in_grid]]) {
  a.write(a.read(t).x + 1u, t);
}
