// EXPECT: valid
// RUN: grid=8 local=8 cases=1 elems=64
// After the closing brace an enumerator has its enum's type, which promotes to int when every value fits:
// A - 2 is -1, not a wrapped unsigned. K = -1, B = 2, so the array has 2 elements: thread 0 writes -8.
#include <metal_stdlib>
using namespace metal;
enum E { A = 1u, B = A + 1 };
constant int K = A - 2;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { float a[B + K + 1]; out[i] = float(K) * 10.0f + float(sizeof(a) / 4); }
