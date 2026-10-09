// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (int(gid)) {
    case 0: out[gid] = 5; break;
    default: int x = 6; out[gid] = x; break;
  }
}
