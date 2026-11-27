// EXPECT: error no matching function for call to "f"
// Refs #341: T is float from one argument and int from the other, so deduction fails and the
// template is no candidate (as in C++), which leaves nothing to call.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x, T y) { return 1.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1, 2.0f); }
