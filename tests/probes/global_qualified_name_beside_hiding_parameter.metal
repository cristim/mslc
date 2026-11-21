// EXPECT: valid
// DISASM: OpFAdd %float %float_1
// A helper parameter k hides the file-scope k; "::k" still names the file-scope one.
#include <metal_stdlib>
using namespace metal;
constant float k = 1.0;
float g(float k) { return ::k + k; }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = g(4.0); }
