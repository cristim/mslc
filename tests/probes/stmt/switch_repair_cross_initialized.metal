// EXPECT: error switch label bypasses variable initialization
#include <metal_stdlib>
using namespace metal;
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (int(gid)) {
    case 0: int x = 5; out[gid] = x; break;
    default: out[gid] = 6; break;
  }
}
