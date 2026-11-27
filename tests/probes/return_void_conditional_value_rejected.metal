// EXPECT: error "v" returns void, so its return cannot carry a value
// Each value of a returned conditional is a return operand, so a float in one of them is refused.
#include <metal_stdlib>
using namespace metal;
inline void v(device float* o) { return true ? (void)0 : 1.0f; }
kernel void k(device float* out [[buffer(0)]]) { v(out); }
