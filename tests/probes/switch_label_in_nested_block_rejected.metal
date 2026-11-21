// EXPECT: error a 'default' label nested inside a compound statement, loop or if within a switch body is not supported

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  int r = 0;
  switch (out[gid]) {
    case 0: r = 1; break;
    { default: r = 3; }
  }
  out[gid] = r;
}
