// EXPECT: error a non-type template parameter of that type is not lowered
// Refs #341: a template's parameters are read when it is declared; a constant that is not an integer
// or a bool (here a float) is a template mslc cannot instantiate.
#include <metal_stdlib>
using namespace metal;
template <float F> float scaled(float x) { return x * F; }
kernel void k(device float* out [[buffer(0)]]) { out[0] = 1.0f; }
