// EXPECT: error pointer alias
// Apple: accepts (const makes the pointer const); mslc refuses a qualifier written beside a pointer alias (#251)
#include <metal_stdlib>
using namespace metal;
using P = device float*;
kernel void using_alias_of_pointer_const_prefix_rejected(device float *o [[buffer(0)]]) {
  const P q = o;
  q[0] = 1.0;
}
