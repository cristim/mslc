// EXPECT: error char pointers, char arrays and bool-from-pointer are not lowered
// Apple accepts const constant char *p = "abc"; mslc rejects it by design (known-issues.md, #156).
#include <metal_stdlib>
using namespace metal;
kernel void string_literal_char_pointer_rejected(device float *o [[buffer(0)]]) {
  const constant char *p = "abc";
  o[0] = 1;
}
