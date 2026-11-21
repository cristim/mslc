// EXPECT: error redefinition of the function template "f"
// Refs #341: a template defined twice with one signature (a prototype and its definition are one).
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(T x) { return 1.0f; }
template <typename T> float f(T x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
