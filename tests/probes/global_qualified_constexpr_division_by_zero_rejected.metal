// EXPECT: error constexpr variable "m" must be initialized by a constant expression
// 1 / (::n - 3) is a division by zero with the file-scope n (3); Apple rejects it as not a
// constant expression, though the local constexpr n (7) would make it 1/4.
#include <metal_stdlib>
using namespace metal;
constant int n = 3;
kernel void kern(device int* out [[buffer(0)]], uint tid [[thread_position_in_grid]]) { int r = 0; constexpr int n = 7; constexpr int m = 1 / (::n - 3); out[0] = m; }
