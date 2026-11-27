// EXPECT: valid
// A half texture is a float storage image: a half4 is widened and a number is narrowed to half first.
// DISASM-MATCH: OpTypeImage %float 2D 0 0 0 2 Unknown
// DISASM: OpFunctionCall %void
// DISASM: OpFConvert %half
// DISASM: OpFConvert %v4float
// DISASM: OpImageWrite
#include <metal_stdlib>
using namespace metal;
void put(texture2d<half, access::write> t, uint2 p, half4 v) { t.write(v, p); }
kernel void hit(texture2d<half, access::write> dst [[texture(0)]], uint3 g [[thread_position_in_grid]]) {
  put(dst, g.xy, half4(half(g.y), 0.1h, 7.0h, 1.0h));
  dst.write(0.1f, g.xy);
}
