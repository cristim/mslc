// EXPECT: valid
// DISASM: %float_100
// DISASM-NOT: %float_2
// Refs #341: explicit template arguments name a template, so f(float), which would be the better
// match for 7.5f, is not a candidate.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x) { return 100.0f; }
float f(float x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f<int>(7.5f); }
