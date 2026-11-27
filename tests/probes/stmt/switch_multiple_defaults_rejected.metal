// EXPECT: error multiple default labels in one switch

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (int(gid)) {
    default: out[gid] = 1; break;
    default: out[gid] = 2; break;
  }
}
