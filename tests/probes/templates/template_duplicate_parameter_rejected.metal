// EXPECT: error redeclaration of the template parameter "T"
// Refs #341: two parameters of one template cannot share a name.
#include <metal_stdlib>
using namespace metal;
template <typename T, int T> float f(float x) { return x; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
