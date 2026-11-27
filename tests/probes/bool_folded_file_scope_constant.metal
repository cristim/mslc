// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// A file-scope constant folds a promoted bool operand: g is 3.
#include <metal_stdlib>
using namespace metal;
constant int g = true | 2; kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = g; }
