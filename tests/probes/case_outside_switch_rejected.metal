// EXPECT: error 'case' label is outside a switch statement

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  case 1: out[gid] = 1;
}
