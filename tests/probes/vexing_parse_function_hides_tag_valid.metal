// EXPECT: valid
// A function named S hides the struct tag, so S(a) is a call and the declaration is a variable.
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
float S(float x) { return x; }
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  float3 v(S(a));
  o[0] = v.x;
}
