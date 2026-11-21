// EXPECT: error pointer type
// Apple: accepts; mslc refuses loudly (known-issues.md, #251)
#include <metal_stdlib>
using namespace metal;
using P = device float*;
kernel void using_alias_of_pointer_rejected(device float *o [[buffer(0)]]) { o[0] = 1; }
