// EXPECT: error is used as a value
// A buffer pointer is not a value; only a reference parameter reads as one.
#include <metal_stdlib>
using namespace metal;
kernel void buffer_pointer_used_as_value_rejected(device float *o [[buffer(0)]], device float *p [[buffer(1)]]) {
  float x = p;
  o[0] = x;
}
