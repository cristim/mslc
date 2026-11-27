// EXPECT: valid
// RUN: grid=1 local=1
//
// "using T = S;" is the struct S, as "typedef S T;" is: T{..} builds a temporary, T is a parameter
// type, and an alias of a scalar and of a vector name those types. Apple accepts every line.
#include <metal_stdlib>
using namespace metal;
struct S { float a; float b; };
using T = S;
using F = float;
using V = float4;
float sum(T t) { return t.a + t.b; }
kernel void using_alias_of_struct_brace_temporary(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  T t = T{in[0], 2.0};
  o[0] = t.a + t.b;
  o[1] = T{3.0, 4.0}.b;
  o[2] = sum(T{in[0], 5.0});
  F f = 6.0;
  V v = V(f);
  o[3] = f + v.x;
}
