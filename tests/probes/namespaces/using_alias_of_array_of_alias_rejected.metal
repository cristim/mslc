// EXPECT: error an array of references, pointers or resources is not lowered yet
// Apple: accepts (a two-dimensional array); mslc refuses a local array of an array alias (#251)
#include <metal_stdlib>
using namespace metal;
using A = float[4];
kernel void using_alias_of_array_of_alias_rejected(device float *o [[buffer(0)]]) {
  A x[2];
  x[1][1] = 1.0;
  o[0] = x[1][1];
}
