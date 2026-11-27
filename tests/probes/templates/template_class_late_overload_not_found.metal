// EXPECT: error is declared after the class template
// Refs #341: a call in a member whose name has a later declaration is refused: Apple binds g(int) here (a fundamental
// type has no argument-dependent lookup), but the rule cannot tell that from a struct argument, so it refuses both.
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
