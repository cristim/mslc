// EXPECT: error statement requires expression of integer type ('float' invalid)

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (float(gid)) {
    case 0: out[gid] = 1; break;
    default: out[gid] = 2;
  }
}
