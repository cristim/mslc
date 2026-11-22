// EXPECT: valid
// RUN: grid=1 local=1
//
// An alias in a kernel declared inside a namespace. Apple accepts.
#include <metal_stdlib>
using namespace metal;
using T = int;
namespace n {
kernel void using_alias_in_namespace_kernel(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  using T = float;
  T t = in[0];
  o[0] = t + 0.5;
}
}
