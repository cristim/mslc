// EXPECT: error would wrap as an unsigned one
// Inside its enum an enumerator has its initializer's type ([dcl.enum]/5): A is unsigned, so A - 2 wraps and
// B is 1 in C++. The evaluator works in signed arithmetic, so the initializer is refused.
#include <metal_stdlib>
using namespace metal;
enum E { A = 1u + 0, B = (A - 2) > 5 };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { out[i] = float(B) + 10.0f * float(A); }
