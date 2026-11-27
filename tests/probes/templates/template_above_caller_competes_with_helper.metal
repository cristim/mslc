// EXPECT: valid
// DISASM: %float_100
// DISASM-NOT: %float_3
// Refs #341: a template declared above the caller is a candidate, so the one-argument helper does not
// make the two-argument call an arity error: the template takes it.
#include <metal_stdlib>
using namespace metal;
float g(float x) { return 3.0f; }
template <typename T> float g(T a, T b) { return 100.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = g(1.0f, 2.0f); }
