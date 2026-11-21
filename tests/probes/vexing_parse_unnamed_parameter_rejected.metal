// EXPECT: error most vexing parse
// float() is an unnamed parameter of function type.
// Apple and C++ read the declaration as a function declaration and reject the use of v as a value.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]]) {
  float3 v(float());
  o[0] = v.x;
}
