// EXPECT: valid
// A pointer to another array, and a value argument that only reads (builtin call), share nothing with the
// element passed by reference.
#include <metal_stdlib>
using namespace metal;
float g(thread float &x, thread float *p, float z) { p[0] = 5.0f; return x + z; }
kernel void helper_reference_element_and_other_array_accepted(device float *o [[buffer(0)]]) {
  float a[2] = {1.0f, 2.0f};
  float b[2] = {3.0f, 4.0f};
  o[0] = g(a[0], b, sin(a[1]));
}
