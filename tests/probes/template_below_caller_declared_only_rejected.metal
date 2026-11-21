// EXPECT: error "g" is declared but never defined
// Refs #341: a declaration with no definition is still refused when a template of its name is declared below.
#include <metal_stdlib>
using namespace metal;
float g(float x);
kernel void k(device float* out [[buffer(0)]]) { out[0] = g(1.0f); }
template <typename T> T g(T a, T b) { return a + b; }
