// EXPECT: valid
// RUN: grid=1 local=1
//
// An alias in a function inside a namespace, with no outer T. Apple accepts.
#include <metal_stdlib>
using namespace metal;
namespace n {
float f(float v) { using T = float; T t = v; return t + 1.0; }
}
kernel void using_alias_in_namespace_function(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  o[0] = n::f(in[0]);
}
