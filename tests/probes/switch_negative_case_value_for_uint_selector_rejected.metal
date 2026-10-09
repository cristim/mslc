// EXPECT: error case value evaluates to -1, which cannot be narrowed to type 'uint'

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (gid) {
    case -1: out[gid] = 1; break;
    default: out[gid] = 2;
  }
}
