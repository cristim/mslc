// EXPECT: error a const written after a pointer alias
// Apple: accepts (a const pointer); mslc refuses a const after a pointer alias (#251)
#include <metal_stdlib>
using namespace metal;
using P = device float*;
float f(P const q) { return q[0]; }
kernel void using_alias_of_pointer_const_after_rejected(device float *o [[buffer(0)]]) {
  o[0] = f(o);
}
