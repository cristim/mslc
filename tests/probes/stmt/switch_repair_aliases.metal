// EXPECT: valid
// DISASM-MATCH: OpSwitch %[0-9]+ %[0-9]+ 4 %[0-9]+
#include <metal_stdlib>
using namespace metal;
constant int label = 1;
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  const uchar narrowed = 258;
  const int label = narrowed + 1;
  const int alias = label + 1;
  switch (int(gid)) {
    case alias: out[gid] = 5; break;
    default: out[gid] = 6; break;
  }
}
