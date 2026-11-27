// EXPECT: valid
// DISASM: %float_1_5
// A conditional of two structs of namespace A is a value of that struct, so A::f is found for it
// (Apple accepts; it was a "not lowered yet" refusal).
#include <metal_stdlib>
using namespace metal;
namespace A { struct S { float v; }; float f(S x) { return 1.5f; } }
kernel void k(device float* out [[buffer(0)]]) {
	A::S a; A::S b; a.v = 1.0f; b.v = 2.0f;
	out[0] = f(out[1] > 0.0f ? a : b);
}
