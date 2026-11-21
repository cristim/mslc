// EXPECT: valid
// A plain variable in parentheses is a variable.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  float3 v(a);
  o[0] = v.x;
}
