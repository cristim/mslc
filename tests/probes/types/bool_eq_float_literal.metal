// EXPECT: valid
// DISASM: OpSelect
// DISASM: OpFOrdEqual

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]]) {
  bool b = out[1] != 0;
  out[0] = (b == 1.0f);
}
