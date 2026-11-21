// EXPECT: error "g" is called before it is declared
// Refs #341: Apple reports an undeclared identifier; a template of that name below does not change it.
#include <metal_stdlib>
using namespace metal;
float h(float y) { return g(y); }
float g(float x) { return x + 3.0f; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = h(1.0f); }
template <typename T> T g(T a, T b, T c) { return a; }
