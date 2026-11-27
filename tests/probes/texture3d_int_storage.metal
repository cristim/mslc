// EXPECT: valid
// DISASM-MATCH: OpTypeImage %int 3D 0 0 0 2 Unknown
// DISASM-MATCH: OpTypeImage %uint 2D 2 1 0 1 Unknown
// An integer element works on every dimension.
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture3d<int, access::read_write> a [[texture(0)]], texture2d_array<ushort, access::read> b [[texture(1)]], uint3 t [[thread_position_in_grid]]) {
  a.write(a.read(t) + int4(b.read(t.xy, t.z).x), t);
}
