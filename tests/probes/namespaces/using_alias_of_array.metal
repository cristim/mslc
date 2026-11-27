// EXPECT: valid
// RUN: grid=1 local=1
//
// "using A = float[4];" names the array type: a local and
// sizeof. Apple accepts every line.
#include <metal_stdlib>
using namespace metal;
using A = float[4];
struct W { float m[4]; float z; };
kernel void using_alias_of_array(device float *o [[buffer(0)]], constant float *in [[buffer(1)]]) {
  A x = {in[0], 2.0, 3.0, 4.0};
  W w;
  w.m[1] = 5.0;
  w.z = 6.0;
  o[0] = x[0] + x[3];
  o[1] = sizeof(A) + sizeof(W);
  o[2] = w.m[1] + w.z;
  x[2] = 9.0;
  o[3] = x[2] + x[1];
}
