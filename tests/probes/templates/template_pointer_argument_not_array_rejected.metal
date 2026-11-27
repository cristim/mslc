// EXPECT: error neither a local array nor a whole buffer
// Refs #341: T* deduces from a local array or a whole buffer only; &a would deduce T = float in Apple, and dropping
// the template instead would let the call fail later as "no matching function", so it is refused where it is deduced.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(thread T* p) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]]) { float a = 1.0f; out[0] = f(&a); }
