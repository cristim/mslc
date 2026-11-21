// EXPECT: error array type
// Apple: accepts; mslc refuses loudly (known-issues.md, #251)
#include <metal_stdlib>
using namespace metal;
using A = float[4];
kernel void using_alias_of_array_rejected(device float *o [[buffer(0)]]) { o[0] = 1; }
