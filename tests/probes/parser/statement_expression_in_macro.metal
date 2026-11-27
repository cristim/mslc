// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpReturn
// The shape a macro writes: "({ ...; index++; })" as a statement, including a return inside it.
#include <metal_stdlib>
using namespace metal;
#define PUSH(value) ({ if (index < 4u) { out[i * 4u + index] = (value); } index++; })
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  uint index = 0;
  PUSH(i);
  PUSH(i + 1u);
  ({ if (i == 3u) { return; } });
  PUSH(i + 2u);
}
