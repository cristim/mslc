// EXPECT: valid
// RUN: grid=1 local=1
//
// "using T = S;" inside a function is the same alias as at file scope, for the rest of its block.
// Two functions may give one name two different types. Apple accepts every line.
#include <metal_stdlib>
using namespace metal;
struct S { float a; float b; };
struct R { float c; };
float f(constant float *in) {
  using T = R;
  T r = T{in[0] + 1.0};
  return r.c + sizeof(T);
}
kernel void using_alias_in_function(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  using T = S;
  using V = float4;
  T t = T{in[0], 2.0};
  o[0] = t.a + t.b;
  o[1] = T{3.0, 4.0}.b;
  o[2] = f(in) + sizeof(T);
  V v = V(in[0]);
  o[3] = v.x + sizeof(V);
  {
    using F = float;
    F x = 7.0;
    o[4] = x;
  }
  o[5] = T{5.0, 6.0}.a;
}
