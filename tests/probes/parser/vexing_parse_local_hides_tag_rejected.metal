// EXPECT: error function "S" is not a builtin
// A local named S hides the struct tag, so S(a) calls a float: Apple says "called object type 'float' is
// not a function". It is not a function declaration, so the vexing-parse refusal must not fire.
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
kernel void k(device float* o [[buffer(0)]]) {
  float a = 1.0;
  float S = 2.0;
  float3 v(S(a));
  o[0] = v.x;
}
