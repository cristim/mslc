// EXPECT: valid
// Two structs with the same layout are two types: each call picks the overload of its own.
#include <metal_stdlib>
using namespace metal;
struct A { float v; };
struct B { float v; };
float f(A a) { return a.v; }
float f(B b) { return b.v * 10.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	A a; a.v = 1.0f;
	B b; b.v = 2.0f;
	out[i] = f(a) + f(b);
}
