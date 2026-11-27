// EXPECT: error most vexing parse
// Two TYPE(name) groups are still a parameter list.
// Apple and C++ read the declaration as a function declaration and reject the use of v as a value.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  int b = 2;
  float3 v(float(a), float(b));
  o[0] = v.x;
}
