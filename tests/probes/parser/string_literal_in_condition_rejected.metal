// EXPECT: error mslc has no string type, so char pointers
// Apple accepts if ("x"); mslc rejects it by design (known-issues.md, #156).
#include <metal_stdlib>
using namespace metal;
kernel void string_literal_in_condition_rejected(device float *o [[buffer(0)]]) {
  if ("x") o[0] = 1;
}
