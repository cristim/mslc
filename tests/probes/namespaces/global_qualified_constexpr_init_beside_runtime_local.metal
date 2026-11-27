// EXPECT: valid
// RUN: grid=1 local=1
// constexpr int m = ::n reads the file-scope n even though a runtime local n hides it.
#include <metal_stdlib>
using namespace metal;
constant int n = 3; kernel void kern(device int* out [[buffer(0)]], uint tid [[thread_position_in_grid]]) { int n = int(tid) + 7; constexpr int m = ::n; out[0] = m + n; }
