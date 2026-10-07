// EXPECT: error duplicate case value '1'

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (int(gid)) {
    case 1: out[gid] = 1; break;
    case 1: out[gid] = 2; break;
  }
}
