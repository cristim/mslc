// EXPECT: valid
// DISASM-MATCH: OpTypeImage %uint 2D 0 0 0 2 Unknown
// DISASM: OpImageWrite
#include <metal_stdlib>
using namespace metal;
kernel void hit(const metal::texture2d<unsigned int, metal::access::write> dst) {
  dst.write(0xffffffffu, uint2(0));
}
