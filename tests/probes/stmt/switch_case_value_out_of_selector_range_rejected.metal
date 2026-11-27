// EXPECT: error case value evaluates to 4294967295, which cannot be narrowed to type 'int'

#include <metal_stdlib>
using namespace metal;

kernel void k(device int* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  switch (int(gid)) {
    case 4294967295u: out[gid] = 1; break;
    default: out[gid] = 2;
  }
}
