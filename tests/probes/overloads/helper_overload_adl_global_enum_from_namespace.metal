// EXPECT: valid
// DISASM: %float_1_5
// DISASM-NOT: %float_7_5
// An argument whose type is declared at file scope associates the file scope: inside namespace A,
// f(ea) finds the file's f(E) beside A::f(float), and f(E) is exact (Apple: 1.5). Only the
// functions a call reaches are emitted, so the other one's constant is absent.
#include <metal_stdlib>
using namespace metal;
enum E { ea };
float f(E x) { return 1.5f; }
namespace A { float f(float x) { return 7.5f; } float g() { return f(ea); } }
kernel void k(device float* out [[buffer(0)]]) { out[0] = A::g(); }
