// EXPECT: valid
// A local named as_type is not the library's as_type<T>: "as_type < 4" compares it.
#include <metal_stdlib>
using namespace metal;
kernel void k(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  int as_type = int(i);
  out[i] = as_type < 4 ? 1 : 2;
}
