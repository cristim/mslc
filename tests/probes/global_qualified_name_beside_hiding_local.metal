// EXPECT: valid
// DISASM: OpFAdd %float %float_1
// DISASM-NO-MATCH: OpFAdd %float %[0-9]+ %[0-9]+
// "::k" names the file-scope constant (1) while the local k (2) is in scope, so the sum
// is the constant plus a load of the local, not two loads of the local.
// Apple accepts this; it was rejected until the AST marked the file-scope name.
#include <metal_stdlib>
using namespace metal;
constant float k = 1.0;
kernel void kern(device float* out [[buffer(0)]]) { float k = 2.0; out[0] = ::k + k; }
