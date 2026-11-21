// EXPECT: valid
// An operator after the cast, or a call that is not a type, is an expression: a variable.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  float3 v(float(a) + 1.0);
  float3 w(abs(a));
  float3 x(float3(a) + 0);
  float3 y(float(a), 1.0, 2.0);
  o[0] = v.x + w.x + x.x + y.x;
}
