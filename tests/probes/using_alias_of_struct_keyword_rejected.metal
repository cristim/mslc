// EXPECT: error keyword in a type alias
// Apple: accepts; mslc refuses loudly (known-issues.md, #251)
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
using T = struct S;
kernel void using_alias_of_struct_keyword_rejected(device float *o [[buffer(0)]]) { o[0] = 1; }
