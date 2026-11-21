// EXPECT: valid
// DIFFCHECK: writes-zero
// 1 / (::n - 7) with the file-scope n (3) is a constant 1 / -4; the local constexpr n (7)
// would make it a division by zero. Apple accepts it.
#include <metal_stdlib>
using namespace metal;
constant int n = 3;
kernel void kern(device int* out [[buffer(0)]], uint tid [[thread_position_in_grid]]) { int r = 0; constexpr int n = 7; constexpr int m = 1 / (::n - 7); out[0] = m; }
