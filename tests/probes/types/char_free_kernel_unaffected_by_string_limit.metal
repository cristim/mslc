// EXPECT: valid
// The string and pointer-to-bool rejections do not touch an ordinary char-free kernel (#156).
#include <metal_stdlib>
using namespace metal;
kernel void char_free_kernel_unaffected_by_string_limit(device float *o [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  bool b = i > 1u;
  if (b) o[i] = 1.0;
  else o[i] = 2.0;
}
