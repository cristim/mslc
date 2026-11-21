// EXPECT: valid
// For a non-const variable the mutable reference is the better binding; a const variable and
// an rvalue (even one converted from an int) can only take the const reference.
#include <metal_stdlib>
using namespace metal;
float f(thread float &x) { x += 1.0f; return 1.0f; }
float f(const thread float &x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	float a = 1.0f;
	const float c = 5.0f;
	out[i] = f(a) + f(c) + f(1.0f) + f(2);
}
