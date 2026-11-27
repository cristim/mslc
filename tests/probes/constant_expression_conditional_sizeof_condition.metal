// EXPECT: valid
// RUN: grid=8 local=8 cases=1 elems=64
// A sizeof in the condition does not make the conditional unsigned: only its values set its type. A is 1 - 3,
// thread 0 writes -2.
#include <metal_stdlib>
using namespace metal;
enum E { A = (sizeof(int) == 4 ? 1 : 2) - 3 };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = float(A); }
