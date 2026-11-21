// EXPECT: valid
// DISASM-MATCH: OpAccessChain %_ptr_Function_v2float %[0-9]+ %[0-9]+
// "::m[m][0]": a file-scope matrix column chosen by a local int of the same name.
#include <metal_stdlib>
using namespace metal;
constant float2x2 m = float2x2(1,2,3,4); kernel void kern(device float* out [[buffer(0)]]) { int m = 1; out[0] = ::m[m][0]; }
