// EXPECT: valid
// RUN: grid=1 local=1
// DISASM: OpCompositeConstruct %v2bool
// DISASM: OpLogicalEqual %v2bool

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]]) {
  bool2 a = bool2(out[1] != 0, out[2] != 0);
  bool s = out[3] != 0;
  bool2 c = (a == s);
  bool2 d = (s != a);
  out[0] = c.x ? 1 : 0;
  out[2] = d.y ? 1 : 0;
}
