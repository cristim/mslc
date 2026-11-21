// EXPECT: error a pointer argument other than a local array
// Apple takes &x for a thread float* parameter; mslc lowers a local array only, and a choice
// among overloads would have to guess, so it is refused.
#include <metal_stdlib>
using namespace metal;
float g(thread float *p) { return p[0]; }
float g(thread int *p) { return 0.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	float a = 1.0f;
	out[i] = g(&a);
}
