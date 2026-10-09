// EXPECT: error case value is not a constant expression

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  int n = int(gid);
  switch (n) {
    case n: out[gid] = 1; break;
    default: out[gid] = 2;
  }
}
