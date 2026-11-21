// EXPECT: valid
// A texture1d storage image is Dim 1D and needs the Image1D capability.
// DISASM-MATCH: OpTypeImage %float 1D 0 0 0 2 Unknown
// DISASM: OpCapability Image1D
// DISASM-MATCH: OpImageRead %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+
// DISASM: OpImageWrite
// DISASM-MATCH: OpImageQuerySize %uint
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture1d<float, access::read_write> a [[texture(0)]], texture1d<half, access::write> w [[texture(1)]], uint t [[thread_position_in_grid]]) {
  a.write(a.read(t), t);
  w.write(half4(1), t);
  w.write(float(a.get_width()), ushort(t));
}
