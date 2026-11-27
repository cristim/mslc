// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM-MATCH: OpExtInst %v2int %[0-9]+ FindUMsb
// DISASM-NOT: OpFDiv
// c / max is the correctly rounded quotient, made from its repeating bits rather than by a division
// (8.7.1.2: Metal 2's normalized conversions round correctly), so c / 65535 times 65535 is c again.
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  out[i] = uint(unpack_unorm2x16_to_float(i).x * 65535.0f) ^ i;
}
