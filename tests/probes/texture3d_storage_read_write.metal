// EXPECT: valid
// DISASM-MATCH: OpTypeImage %float 3D 0 0 0 2 Unknown
// DISASM-MATCH: OpTypeImage %uint 3D 2 0 0 1 Unknown
// DISASM-MATCH: OpImageRead %v4float %[0-9A-Za-z_]+ %[0-9A-Za-z_]+
// DISASM: OpImageWrite
// DISASM: OpImageQuerySize %v3uint
// DISASM: OpImageQuerySizeLod %v3uint
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture3d<float, access::read_write> a [[texture(0)]], texture3d<uint, access::read> r [[texture(1)]], texture3d<float, access::write> w [[texture(2)]],
    device uint* out [[buffer(0)]], uint3 t [[thread_position_in_grid]]) {
  a.write(a.read(t), t);
  w.write(float4(1), ushort3(t));
  out[0] = a.get_depth() + r.get_depth() + w.get_width() + w.get_height() + r.read(t).x;
}
