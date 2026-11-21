// EXPECT: valid
// DISASM: %float_1_5
// A comma expression has the type of its right operand, so a struct of namespace A there searches A.
#include <metal_stdlib>
using namespace metal;
namespace A { struct S { float v; }; float f(S x) { return 1.5f; } }
kernel void k(device float* out [[buffer(0)]]) {
	A::S a; a.v = 1.0f;
	out[0] = f((out[1], a));
}
