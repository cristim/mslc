// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
struct S { int x; };
kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (int(gid)) {
    case 0: S s; s.x = 5; out[gid] = s.x; break;
    default: out[gid] = 6; break;
  }
}
