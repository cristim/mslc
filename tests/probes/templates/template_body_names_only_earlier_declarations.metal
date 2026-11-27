// EXPECT: valid
// DISASM: %float_3
// DISASM-NOT: %float_5
// Refs #341: the body's non-dependent lookup is at the template's declaration: h(float) is the only
// h there, so the h(int) declared below the template is not a candidate and f(1) calls h(float).
#include <metal_stdlib>
using namespace metal;
float h(float x) { return 3.0f; }
template <typename T> float f(T x) { return h(x); }
float h(int x) { return 5.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1); }
