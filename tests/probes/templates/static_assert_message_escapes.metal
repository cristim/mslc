// EXPECT: error static assertion failed: a "quoted" word (line 6)
// The message is the string literal's value, its escapes read.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
  static_assert(sizeof(int) == 2, "a \"quoted\" word");
  out[i] = 1; }
