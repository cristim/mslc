// EXPECT: valid
// A half texture reads as a float image and is narrowed after the read.
// DISASM-MATCH: OpTypeImage %float 2D 0 0 0 2 Unknown
// DISASM-MATCH: OpImageRead %v4float
// DISASM-MATCH: OpFConvert %v4half
// DISASM: NonWritable
#include <metal_stdlib>
using namespace metal;
fragment half4 hit(texture2d<half, access::read> src [[texture(0)]], float4 p [[position]]) {
  return src.read(uint2(p.xy));
}
