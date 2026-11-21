// EXPECT: valid
// DISASM: %float_100
// DISASM-NOT: %float_2
// Refs #341: with an int argument the template (T = int, an exact match) beats f(float), which
// needs a conversion, so the function's body (2.0f) is never emitted.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x) { return 100.0f; }
float f(float x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1); }
