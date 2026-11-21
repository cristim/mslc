// EXPECT: error so char pointers, char arrays
// Apple accepts "a" "b"; mslc rejects it by design (known-issues.md, #156).
#include <metal_stdlib>
using namespace metal;
kernel void string_literal_concatenation_rejected(device float *o [[buffer(0)]]) {
  const constant char *p = "a" "b";
  o[0] = 1;
}
