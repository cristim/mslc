// EXPECT: valid
// DISASM: OpIMul %int %int_3 %int_2
// A constexpr local initialised from "::n * 2" reads the file-scope n (3), though a runtime
// local n hides it: Apple stores 6.
#include <metal_stdlib>
using namespace metal;
constant int n = 3; kernel void kern(device int* out [[buffer(0)]], uint tid [[thread_position_in_grid]]) { int n = int(tid); constexpr int m = ::n * 2; out[0] = m; }
