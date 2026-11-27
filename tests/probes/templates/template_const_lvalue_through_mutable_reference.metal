// EXPECT: valid
// DISASM: OpFunctionCall %float
// Refs #341: Apple deduces T = const float for a const lvalue bound to thread T&, and the
// instance takes a reference to const.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(thread T& x) { return float(x) + 1.0f; }
kernel void k(device float* out [[buffer(0)]]) { const float c = 1.0f; out[0] = f(c); }
