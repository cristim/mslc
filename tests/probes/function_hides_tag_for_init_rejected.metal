// EXPECT: error after a for initialiser
// Apple: the function S hides the struct tag, so "S s" in a for initialiser is not a declaration.
#include <metal_stdlib>
using namespace metal;
struct R { float v; };
struct S { float v; };
R S(float x) { R r; r.v = x + 100.0f; return r; }
kernel void function_hides_tag_for_init_rejected(device float* out [[buffer(0)]])
{ for (S s = S(1.0f); s.v < 0.0f; ) { out[0] = 1.0f; } }
