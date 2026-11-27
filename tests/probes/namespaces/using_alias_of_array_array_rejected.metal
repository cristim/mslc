// EXPECT: error a pointer to an array alias, and an array of one
// Apple: accepts (a two-dimensional array); mslc refuses it (#251)
#include <metal_stdlib>
using namespace metal;
using A = float[4];
kernel void using_alias_of_array_array_rejected(device float *o [[buffer(0)]]) {
  o[0] = sizeof(A[2]);
}
