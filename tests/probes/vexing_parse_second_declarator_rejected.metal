// EXPECT: error most vexing parse
// The second declarator is the vexing one.
// Apple and C++ read the declaration as a function declaration and reject the use of v as a value.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  float3 w(1.0), v(float(a));
  o[0] = v.x;
}
