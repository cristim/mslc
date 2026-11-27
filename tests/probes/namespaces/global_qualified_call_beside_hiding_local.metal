// EXPECT: valid
// DISASM: OpFunctionCall
// A local f hides the file-scope helper f; "::f()" still calls the helper.
#include <metal_stdlib>
using namespace metal;
float f() { return 3.0; }
kernel void kern(device float* out [[buffer(0)]]) { float f = 2.0; out[0] = ::f() + f; }
