// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
namespace A::B { constant float b = 2.0; float f(float x) { return x + b; } }
namespace A { float g(float x) { return B::f(x); } }
kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::B::f(1.0) + A::g(1.0); }
