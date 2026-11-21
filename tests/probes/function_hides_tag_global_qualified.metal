// EXPECT: valid
// DISASM: %float_100
// DISASM-NOT: %float_200
#include <metal_stdlib>
using namespace metal;
struct R { float v; };
struct S { float v; S(float x) : v(x + 200.0f) {} };
R S(float x) { R r; r.v = x + 100.0f; return r; }
kernel void function_hides_tag_global_qualified(device float* out [[buffer(0)]]) { out[0] = ::S(1.0f).v; }
