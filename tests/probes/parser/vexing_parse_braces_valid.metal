// EXPECT: valid
// Braces never declare a function.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  float3 v{float(a)};
  o[0] = v.x;
}
