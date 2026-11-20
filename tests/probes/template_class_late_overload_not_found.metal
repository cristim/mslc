// EXPECT: valid
// DISASM: OpConvertFToS
// Refs #341: a dependent call in a member binds what is declared above the template (g(int), the float converting),
// not g(float) written below it; Apple does the same for a fundamental type, which has no argument-dependent lookup.
#include <metal_stdlib>
using namespace metal;
float g(int x) { return 1.0f; }
template <typename T> struct P { T v; float f() const { return g(v); } };
float g(float x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	P<float> p;
	p.v = 2.5f;
	out[i] = p.f();
}
