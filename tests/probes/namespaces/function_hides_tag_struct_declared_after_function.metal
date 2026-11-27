// EXPECT: valid
// DISASM: %float_100
// DISASM-NOT: %float_200
#include <metal_stdlib>
using namespace metal;
struct R { float v; };
R S(float x) { R r; r.v = x + 100.0f; return r; }
struct S { float v; S(float x) : v(x + 200.0f) {} };
kernel void function_hides_tag_struct_declared_after_function(device float* out [[buffer(0)]]) { out[0] = S(1.0f).v; }
