// EXPECT: error most vexing parse
// A typedef name as the declared type.
// Apple and C++ read the declaration as a function declaration and reject the use of v as a value.
#include <metal_stdlib>
using namespace metal;
typedef float3 F3;
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  F3 v(float(a));
  o[0] = v.x;
}
