// EXPECT: valid
// DISASM: %float_1_5
// DISASM-NOT: %float_7_5
// A using declaration in a block does not switch argument-dependent lookup off (only a local
// function declaration does): f(ea) still finds the file's f(E), exact (Apple: 1.5).
#include <metal_stdlib>
using namespace metal;
enum E { ea };
float f(E x) { return 1.5f; }
namespace A { float f(int x) { return 7.5f; } float f(float x) { return 7.5f; } }
kernel void k(device float* out [[buffer(0)]])
{
	using A::f;
	out[0] = f(ea);
}
