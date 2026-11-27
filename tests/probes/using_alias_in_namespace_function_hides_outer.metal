// EXPECT: valid
// RUN: grid=1 local=1
// DISASM-NOT: OpConvertFToS
//
// An alias in a function inside a namespace is the function's own T, not the outer T. Apple accepts.
#include <metal_stdlib>
using namespace metal;
using T = int;
namespace n {
float f(float v) { using T = float; T t = v; return t; }
}
kernel void using_alias_in_namespace_function_hides_outer(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  T i = 2;
  o[0] = n::f(in[0]) + 0.25 + i;
}
