// EXPECT: error inside a function
// Apple: accepts; mslc refuses loudly (known-issues.md, #251)
#include <metal_stdlib>
using namespace metal;
struct S { float a; }; struct R { float b; };
float f(device float *o) { using T = S; T t = T{1.0}; return t.a + o[0]; }
kernel void using_alias_in_function_rejected(device float *o [[buffer(0)]]) { using T = R; T r = T{2.0}; o[0] = r.b + f(o); }
