// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// A local named vec is not the library's vec<T, N>: a statement starting "vec < ..." is an expression.
#include <metal_stdlib>
using namespace metal;
kernel void k(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  int vec = int(i);
  int n = 0;
  vec < 4;
  out[i] = n + vec;
}
