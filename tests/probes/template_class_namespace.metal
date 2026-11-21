// EXPECT: valid
// DISASM-MATCH: OpFunctionCall %float
// Refs #341: an instance is declared in the template's namespace.
#include <metal_stdlib>
using namespace metal;
namespace A { template <typename T> struct Box { T v; Box(T x) : v(x) {} T get() const { return v; } }; }
using namespace A;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	A::Box<float> a(1.0f);
	Box<float> b(2.0f);
	out[i] = a.get() + b.get();
}
