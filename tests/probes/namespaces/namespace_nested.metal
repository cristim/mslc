// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
namespace A {
  constant float a = 1.0;
  namespace B {
    constant float b = 2.0;
    float f(float x) { return x + a + b; }
  }
  float g(float x) { return B::f(x) + B::b; }
}
kernel void kern(device float* out [[buffer(0)]]) { out[0] = A::B::f(1.0) + A::g(1.0) + A::B::b; }
