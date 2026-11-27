// EXPECT: valid
// DISASM-MATCH: OpTypeImage %float 2D 2 0 0 1 Unknown
// DISASM-MATCH: OpImageFetch %v4float
// DISASM-MATCH: OpFConvert %v4half
// DISASM-NOT: NonWritable
// A half texture reads as a float image and is narrowed after the read.
#include <metal_stdlib>
using namespace metal;
fragment half4 hit(texture2d<half, access::read> src [[texture(0)]], float4 p [[position]]) {
  return src.read(uint2(p.xy));
}
