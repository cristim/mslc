// EXPECT: valid
// DISASM: %float_2
// DISASM-NOT: %float_100
// Refs #341: a function and a template that convert the argument alike are a tie, which the
// function wins ([over.match.best]); Apple calls f(float) here, and the template is never
// instantiated, so its body (100.0f) is not emitted.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x) { return 100.0f; }
float f(float x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
