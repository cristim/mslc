// EXPECT: error is evaluated to choose among overloads and is also bound to a reference by one
// The argument is evaluated once to learn its type and again to bind the reference, so it
// must not do anything twice.
#include <metal_stdlib>
using namespace metal;
float f(thread float &x) { return 1.0f; }
float f(float3 x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	float a[2] = { 1.0f, 2.0f };
	int j = 0;
	out[i] = f(a[j++]);
}
