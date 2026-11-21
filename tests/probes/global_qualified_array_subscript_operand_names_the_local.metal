// EXPECT: valid
// DISASM-MATCH: OpAccessChain %_ptr_Private_float %[0-9]+ %[0-9]+
// "::a[int(a[0]) - 9]": the index reads the local a (10 - 9 = 1), so the file-scope array
// is indexed by a runtime value, not by the constant 0 the file-scope a[0] would give.
#include <metal_stdlib>
using namespace metal;
constant float a[2] = {1,2}; kernel void kern(device float* out [[buffer(0)]], uint tid [[thread_position_in_grid]]) { float a[2] = {10,20}; out[0] = ::a[int(a[0]) - 9]; }
