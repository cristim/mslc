// EXPECT: valid
// A helper returning S(1.0f) as R: the call of the function, not a struct S converted to R.
// DISASM: %float_100
// DISASM-NOT: %float_200
#include <metal_stdlib>
using namespace metal;
struct R { float v; };
struct S { float v; S(float x) : v(x + 200.0f) {} };
R S(float x) { R r; r.v = x + 100.0f; return r; }
R h() { return S(1.0f); }
kernel void function_hides_tag_in_helper_return(device float* out [[buffer(0)]]) { out[0] = h().v; }
