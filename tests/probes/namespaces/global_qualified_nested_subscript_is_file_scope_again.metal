// EXPECT: valid
// DISASM-MATCH: OpAccessChain %_ptr_Private_float %[0-9]+ %[0-9]+
// A "::" inside the index is a file-scope name again, beside the local a.
#include <metal_stdlib>
using namespace metal;
constant float a[2] = {1,2}; kernel void kern(device float* out [[buffer(0)]]) { float a[2] = {10,20}; out[0] = ::a[1] + a[1] + ::a[::a[0] > 0.0 ? 1 : 0]; }
