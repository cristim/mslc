// EXPECT: error would wrap as an unsigned one
// A is K - 2u, an unsigned value, so (A - 2) > 5 holds in C++ (B = 1). Refused as the evaluator would not wrap.
#include <metal_stdlib>
using namespace metal;
constant uint K = 3u;
enum E { A = K - 2u, B = (A - 2) > 5 };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = float(B) + 10.0f * float(A); }
