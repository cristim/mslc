// EXPECT: error a 'case' label nested inside a compound statement, loop or if within a switch body is not supported

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  int n = out[gid];
  int r = 0;
  switch (n & 3) {
    case 0:
      do {
        r += 1;
    case 3:
        r += 1;
      } while (--n > 0);
  }
  out[gid] = r;
}
