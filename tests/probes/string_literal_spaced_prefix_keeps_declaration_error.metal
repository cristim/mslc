// EXPECT: error after a declaration, found string literal
// A space after the L makes it an identifier and a separate literal, so the prefix message must not fire.
#include <metal_stdlib>
using namespace metal;
kernel void string_literal_spaced_prefix_keeps_declaration_error(device float *o [[buffer(0)]]) {
  const constant char *p = L "abc";
  o[0] = 1;
}
