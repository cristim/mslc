// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// A case label folds a promoted bool operand: thread 2 writes 20.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { switch (int(i & 3)) { case true + 1: out[i] = 20; break; default: out[i] = 0; } }
