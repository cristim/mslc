// EXPECT: error would wrap as an unsigned one
// The whole enumerator initializer is a conditional that is a uint, so A is 4294967295 in C++.
#include <metal_stdlib>
using namespace metal;
enum E { A = false ? 1u : -1 };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = A > 0 ? 1.0f : 2.0f; }
