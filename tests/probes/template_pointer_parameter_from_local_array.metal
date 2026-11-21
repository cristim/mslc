// EXPECT: valid
// DISASM: OpFunctionCall %float
// Refs #341: T* takes the element type of a local array, as a helper's thread pointer takes the array.
#include <metal_stdlib>
using namespace metal;
template <typename T> T first(thread const T* p) { return p[0]; }
kernel void k(device float* out [[buffer(0)]]) { float a[2] = {1.0f, 2.0f}; out[0] = first(a); }
