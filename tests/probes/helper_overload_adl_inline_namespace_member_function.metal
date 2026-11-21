// EXPECT: valid
// DISASM: %float_1_5
// DISASM-NOT: %float_7_5
// A function in an inline namespace of A is a member of A for lookup, so the struct of A finds it.
#include <metal_stdlib>
using namespace metal;
namespace A { struct S { float v; }; inline namespace V { float f(S s, int y) { return 1.5f; } } }
namespace B { float f(A::S s, float y) { return 7.5f; } float g() { A::S s; s.v = 0.0f; return f(s, 1); } }
kernel void k(device float* out [[buffer(0)]]) { out[0] = B::g(); }
