// EXPECT: valid
// DISASM: %float_1_5
// DISASM-NOT: %float_7_5
// S lives in the inline namespace A::V, so A is associated with it too: A::f(S, int) is an exact match
// and beats B::f(A::S, float), which the call inside B finds by ordinary lookup (Apple's pick).
#include <metal_stdlib>
using namespace metal;
namespace A { inline namespace V { struct S { float v; }; } float f(S s, int y) { return 1.5f; } }
namespace B { float f(A::S s, float y) { return 7.5f; } float g() { A::S s; s.v = 0.0f; return f(s, 1); } }
kernel void k(device float* out [[buffer(0)]]) { out[0] = B::g(); }
