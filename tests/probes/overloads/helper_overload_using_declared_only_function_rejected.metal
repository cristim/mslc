// EXPECT: error is declared but never defined
// A using declaration of a function with no body, called in an executable.
#include <metal_stdlib>
using namespace metal;
namespace A { float f(float x); }
using A::f;
kernel void k(device float* out [[buffer(0)]]) { out[0] = f(1.0f); }
