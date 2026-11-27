// EXPECT: valid
// DISASM: OpFunctionCall %float
// DISASM: NMax
// Refs #341: the result of a template call is an argument of a builtin, which no namespace's function
// shares a name with, so nothing depends on its type being known before it is evaluated.
#include <metal_stdlib>
using namespace metal;
template <typename T> T id(T x) { return x; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = max(id(1.0f), 2.0f); }
