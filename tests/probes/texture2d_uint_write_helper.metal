// EXPECT: valid
// DISASM: OpFunctionCall %void
// DISASM: OpImageWrite
// DISASM: OpImageQuerySize %v2uint
#include <metal_stdlib>
using namespace metal;
void store(texture2d<uint, access::write> dst, uint value);
void store(texture2d<uint, access::write> dst, uint value) { dst.write(value, uint2(dst.get_width()-1u, 0u)); }
kernel void hit(texture2d<uint, access::write> a, texture2d<uint, access::write> b) {
  store(a, 16777217u); store(b, 0x80000001u);
}
