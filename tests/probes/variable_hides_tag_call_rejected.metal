// EXPECT: error is not a builtin mslc recognises
// Apple: called object type 'float' is not a function. mslc used to construct the struct.
#include <metal_stdlib>
using namespace metal;
struct S { float v; S(float x) : v(x) {} };
constant float S = 2.0f;
kernel void variable_hides_tag_call_rejected(device float* out [[buffer(0)]]) { out[0] = S(1.0f); }
