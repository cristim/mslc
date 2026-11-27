// EXPECT: valid
// texture2d<int> is a storage image with an int sampled type; reads give int4 and write takes an int4 or a number.
// DISASM-MATCH: OpTypeImage %int 2D 0 0 0 2 Unknown
// DISASM-NOT: OpTypeImage %uint
// DISASM-MATCH: OpImageRead %v4int
// DISASM-NOT: OpSConvert
// DISASM: OpImageWrite
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<int, access::read_write> a [[texture(0)]], texture2d<int, access::write> w [[texture(1)]], uint2 t [[thread_position_in_grid]]) {
  int4 v = a.read(t);
  a.write(v + int4(1), t);
  w.write(-3, t);
  w.write(7u, t);
  w.write(2.5, t);
}
