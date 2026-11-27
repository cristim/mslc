// EXPECT: valid
// A short texture is an int image narrowed after the read and widened before the write; a ushort one the same through uint.
// DISASM-MATCH: OpTypeImage %int 2D 0 0 0 2 Unknown
// DISASM-MATCH: OpTypeImage %uint 2D 0 0 0 2 Unknown
// DISASM-MATCH: OpImageRead %v4int
// DISASM-MATCH: OpSConvert %v4short
// DISASM-MATCH: OpImageRead %v4uint
// DISASM-MATCH: OpUConvert %v4ushort
// DISASM-MATCH: OpSConvert %v4int
// DISASM-MATCH: OpUConvert %v4uint
// DISASM: OpImageWrite
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<short, access::read_write> s [[texture(0)]], texture2d<ushort, access::read_write> u [[texture(1)]], uint2 t [[thread_position_in_grid]]) {
  short4 a = s.read(t);
  ushort4 b = u.read(t);
  s.write(a + short4(1), t);
  u.write(b, t);
  s.write(70000, t);
}
