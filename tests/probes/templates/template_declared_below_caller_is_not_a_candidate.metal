// EXPECT: valid
// DISASM: %float_1
// DISASM-NOT: %float_100
// Refs #341: f(int) is the only f where h calls it; the template declared below is not found (Apple: 1).
#include <metal_stdlib>
using namespace metal;
float f(int x) { return 1.0f; }
float h(float x) { return f(x); }
template <typename T> float f(T x) { return 100.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = h(1.0f); }
