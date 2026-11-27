// EXPECT: error a conversion to bool or a copy)
// Apple accepts a kernel buffer pointer in a condition; mslc has no pointer null test (#156).
#include <metal_stdlib>
using namespace metal;
kernel void buffer_pointer_in_condition_rejected(device float *o [[buffer(0)]], device float *p [[buffer(1)]]) {
  if (p) o[0] = 1;
}
