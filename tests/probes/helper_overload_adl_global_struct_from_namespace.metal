// EXPECT: valid
// DISASM: %float_1_5
// DISASM-NOT: %float_7_5
// As for an enum: a struct of the file scope searches the file scope (Apple: the file's f(S)).
#include <metal_stdlib>
using namespace metal;
struct S { float v; };
float f(S x) { return 1.5f; }
namespace A { float f(float x) { return 7.5f; } float g(S s) { return f(s); } }
kernel void k(device float* out [[buffer(0)]]) { S s; s.v = 1.0f; out[0] = A::g(s); }
