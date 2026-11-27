// EXPECT: error a pointer to an array alias
// Apple: accepts (a pointer to an array); mslc refuses it (#251)
#include <metal_stdlib>
using namespace metal;
using A = float[4];
float f(thread A* p) { return (*p)[0]; }
kernel void using_alias_of_array_pointer_rejected(device float *o [[buffer(0)]]) {
  A x = {1.0, 2.0, 3.0, 4.0};
  o[0] = f(&x);
}
