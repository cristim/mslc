// EXPECT: error case value is not a constant expression
#include <metal_stdlib>
using namespace metal;
constant int label = 1;
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  const int label = label;
  switch (int(gid)) {
    case label: out[gid] = 5; break;
    default: out[gid] = 6; break;
  }
}
