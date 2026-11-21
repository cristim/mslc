// EXPECT: valid
// A uint texture takes a uint4, or any number converted to uint and put in every channel (Apple accepts all three).
// DISASM-MATCH: OpTypeImage %uint 2D 0 0 0 2 Unknown
// DISASM: OpConvertFToU
// DISASM-MATCH: OpImageWrite %[0-9A-Za-z_]+ %[0-9A-Za-z_]+ %[0-9A-Za-z_]+
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<uint, access::write> dst [[texture(0)]], uint2 t [[thread_position_in_grid]], device float* f [[buffer(0)]], device int* i [[buffer(1)]]) {
  dst.write(uint4(1, 2, 3, 4), t);
  dst.write(f[0], t);
  dst.write(i[0], t);
}
