// EXPECT: error a nested namespace definition cannot be inline
// Apple: nested namespace definition cannot be 'inline'.
#include <metal_stdlib>
inline namespace A { constant float v = 1.0; }
inline namespace A::B { }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::v; }
