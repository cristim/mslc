// EXPECT: error so char pointers, char arrays
// Apple accepts sizeof("abc"); mslc rejects it by design (known-issues.md, #156).
#include <metal_stdlib>
using namespace metal;
kernel void string_literal_sizeof_rejected(device float *o [[buffer(0)]]) {
  o[0] = sizeof("abc");
}
