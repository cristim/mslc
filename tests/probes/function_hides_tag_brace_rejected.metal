// EXPECT: error after an expression statement
// Apple: with the function S in scope, S{1.0f} is not a type, so it is rejected.
#include <metal_stdlib>
using namespace metal;
struct R { float v; };
struct S { float v; };
R S(float x) { R r; r.v = x + 100.0f; return r; }
kernel void function_hides_tag_brace_rejected(device float* out [[buffer(0)]]) { out[0] = S{1.0f}.v; }
