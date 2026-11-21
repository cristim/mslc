// EXPECT: valid
// DISASM: OpReturnValue
// Inside N, "::k" is the file-scope k (1), "N::k" is N's (5) and the local k (2) is neither.
#include <metal_stdlib>
using namespace metal;
constant float k = 1.0;
namespace N { constant float k = 5.0; float f() { float k = 2.0; return ::k * 10 + k + N::k; } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = N::f(); }
