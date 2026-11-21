// EXPECT: valid
// DIFFCHECK-RANGE: out 0 1
// DISASM: OpLogicalNotEqual %v2bool

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]]) {
  bool2 a = bool2(out[1] != 0, out[2] != 0);
  bool2 b = bool2(out[3] != 0, true);
  bool2 c = (a != b);
  out[0] = c.x ? 1 : 0;
}
