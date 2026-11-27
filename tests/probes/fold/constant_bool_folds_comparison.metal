// EXPECT: valid
// DISASM-NOT: OpLogicalEqual
// DISASM-MATCH: OpConstant %int 1111[^0-9]

#include <metal_stdlib>
using namespace metal;

constant bool A = true;
constant bool B = A == false;
constant bool C = 2 < 3;
constant int X = int(A != false) + 10 * int(3 <= 3) + 100 * int(-1 < true)
  + 1000 * int(bool(2)) + 10000 * int(3 < 3) + 20000 * int(A == false);

kernel void k(device int* out [[buffer(0)]]) {
  out[0] = B ? 1 : 0;
  out[2] = C ? 1 : 0;
  out[3] = X;
}
