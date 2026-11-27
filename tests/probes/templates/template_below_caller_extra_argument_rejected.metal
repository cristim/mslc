// EXPECT: error call to "g" passes 2 arguments, and it takes 1
// Refs #341: as above, with an argument too many for the helper (invalid SPIR-V before the check).
#include <metal_stdlib>
using namespace metal;
float g(float x) { return x; }
float h(float y) { return g(y, 2.0f); }
kernel void k(device float* out [[buffer(0)]]) { out[0] = h(1.0f); }
template <typename T> T g(T a, T b) { return a + b; }
