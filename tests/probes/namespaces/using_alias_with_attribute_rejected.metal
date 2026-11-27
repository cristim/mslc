// EXPECT: error attribute on a type alias
// Apple: accepts; mslc refuses loudly (known-issues.md, #251)
#include <metal_stdlib>
using namespace metal;
struct S { float a; };
using T [[maybe_unused]] = S;
kernel void using_alias_with_attribute_rejected(device float *o [[buffer(0)]]) { o[0] = 1; }
