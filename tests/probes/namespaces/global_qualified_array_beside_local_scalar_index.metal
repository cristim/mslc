// EXPECT: valid
// DISASM-MATCH: OpAccessChain %_ptr_Private_float %[0-9]+ %[0-9]+
// "::a[a]": the root is the file-scope array and the index is the local int a.
#include <metal_stdlib>
using namespace metal;
constant float a[3] = {1,2,3}; kernel void kern(device float* out [[buffer(0)]]) { int a = 1; out[0] = ::a[a]; }
