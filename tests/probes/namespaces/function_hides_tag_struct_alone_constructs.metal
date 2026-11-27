// EXPECT: valid
// DISASM: %float_200
#include <metal_stdlib>
using namespace metal;
struct R { float v; };
struct S { float v; S(float x) : v(x + 200.0f) {} };
kernel void function_hides_tag_struct_alone_constructs(device float* out [[buffer(0)]]) { out[0] = S(1.0f).v; }
