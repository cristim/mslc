// EXPECT: error the function attribute "convergent" is not lowered
// Refs #341: a template's attributes are read when it is instantiated, and one that is not
// always_inline, noinline, unused or used is refused there as it is on a helper.
#include <metal_stdlib>
using namespace metal;
template <typename T> __attribute__((convergent)) T tc(T x) { return x; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = tc(1.0f); }
