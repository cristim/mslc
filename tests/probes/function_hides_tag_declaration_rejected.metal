// EXPECT: error after an expression statement
// Apple: "must use 'struct' tag to refer to type 'S'" once the function S is in scope.
#include <metal_stdlib>
using namespace metal;
struct R { float v; };
struct S { float v; };
R S(float x) { R r; r.v = x + 100.0f; return r; }
kernel void function_hides_tag_declaration_rejected(device float* out [[buffer(0)]]) { S s{3.0f}; out[0] = s.v; }
