// EXPECT: valid
// RUN: grid=1 local=1
// uchar b[1] = {::n}: the file-scope n (3) is a constant that fits, though a runtime local n hides the name.
#include <metal_stdlib>
using namespace metal;
constant int n = 3; kernel void kern(device int* out [[buffer(0)]], uint tid [[thread_position_in_grid]]) { int n = int(tid); uchar b[1] = {::n}; out[0] = b[0] + n; }
