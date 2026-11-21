// EXPECT: valid
// DISASM: %float_300
// DISASM-NOT: %float_200
#include <metal_stdlib>
using namespace metal;
struct R { float v; };
struct S { float v; S(float x) : v(x + 200.0f) {} };
R S(int x) { R r; r.v = float(x) + 300.0f; return r; }
kernel void function_hides_tag_overload_picks_int(device float* out [[buffer(0)]]) { out[0] = S(1).v; }
