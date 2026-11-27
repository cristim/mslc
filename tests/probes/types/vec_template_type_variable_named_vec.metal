// EXPECT: valid
// A variable named vec is not the library's vec<T, N>: "vec < 3" compares it.
#include <metal_stdlib>
using namespace metal;
constant int vec = 2;
kernel void k(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  int v = int(i);
  out[i] = (vec < 3 ? 1 : 0) + (vec < v ? 10 : 20);
}
