// EXPECT: valid
// Doubled parentheses cannot be a parameter, so this is a variable.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  float3 v((float(a)));
  o[0] = v.x;
}
