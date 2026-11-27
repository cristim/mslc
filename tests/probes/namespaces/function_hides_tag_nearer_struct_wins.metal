// EXPECT: valid
// The struct in the nearer namespace hides the outer function: control that the
// fix does not turn every same-named function into the callee.
// DISASM: %float_200
// DISASM-NOT: %float_100
#include <metal_stdlib>
using namespace metal;
struct R { float v; };
R S(float x) { R r; r.v = x + 100.0f; return r; }
namespace n {
struct S { float v; S(float x) : v(x + 200.0f) {} };
float g() { return S(1.0f).v; }
}
kernel void function_hides_tag_nearer_struct_wins(device float* out [[buffer(0)]]) { out[0] = n::g(); }
