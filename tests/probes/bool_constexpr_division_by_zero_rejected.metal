// EXPECT: error constexpr variable "n" must be initialized by a constant expression
// The folder promotes the bool operands too, so the division by zero is found rather than left to run time.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { constexpr int n = 1 / (true - true); out[i] = n; }
