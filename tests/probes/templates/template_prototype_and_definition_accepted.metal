// EXPECT: valid
// DISASM-NOT: OpFunctionCall
// Refs #341: a prototype followed by its definition is one template, and the kernel never calls it.
#include <metal_stdlib>
using namespace metal;
template <typename T, int N = 2> float f(T x);
template <typename T, int N = 2> float f(T x) { return float(N); }
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
