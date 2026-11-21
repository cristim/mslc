// EXPECT: error so char pointers, char arrays
// Apple accepts "abc"[1]; mslc rejects it by design (known-issues.md, #156).
#include <metal_stdlib>
using namespace metal;
kernel void string_literal_indexed_rejected(device float *o [[buffer(0)]]) {
  char c = "abc"[1];
  o[0] = c;
}
