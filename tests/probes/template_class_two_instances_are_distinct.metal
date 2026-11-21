// EXPECT: valid
// DISASM-CAPTURE: OpFunctionCall %(_struct_[0-9]+) %[_0-9a-zA-Z]+ %float_2_5
// DISASM-MATCH-CAPTURED: OpFunctionCall %$1 %[_0-9a-zA-Z]+ %float_1[^_0-9]
// DISASM-MATCH: OpFunctionCall %_struct_[0-9]+ %[_0-9a-zA-Z]+ %int_3[^_0-9]
// Refs #341: each distinct argument list is one struct with its own member functions.
#include <metal_stdlib>
using namespace metal;
typedef float F;
template <typename T> struct Box { T v; Box(T x) : v(x) {} T get() const { return v; } };
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Box<float> a(2.5f);
	Box<int> b(3);
	Box<F> c(1.0f);
	out[i] = a.get() + float(b.get()) + c.get();
}
