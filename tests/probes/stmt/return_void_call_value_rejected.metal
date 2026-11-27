// EXPECT: error "v" returns void, so its return cannot carry a value
// A call that returns a value is not a void expression.
#include <metal_stdlib>
using namespace metal;
inline float g() { return 1.0f; }
inline void v() { return g(); }
kernel void k(device float* out [[buffer(0)]]) { v(); out[0] = 1; }
