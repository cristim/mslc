// EXPECT: valid
// DIFFCHECK-RANGE: out 0 1
// DISASM: OpSelect
// DISASM: OpIEqual

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]]) {
  bool b = out[1] != 0;
  int i = out[2];
  out[0] = (b == i);
}
