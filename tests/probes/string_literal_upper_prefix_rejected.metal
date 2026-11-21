// EXPECT: error a prefixed string literal is not lowered
// Before: "expected ; after a declaration, found string literal". Apple accepts most prefixed literals; mslc rejects all by design (#156).
#include <metal_stdlib>
using namespace metal;
kernel void string_literal_upper_prefix_rejected(device float *o [[buffer(0)]]) {
  const constant char *p = U"abc";
  o[0] = 1;
}
