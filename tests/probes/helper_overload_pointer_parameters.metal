// EXPECT: valid
// A pointer parameter takes a local array of its element type, never another element type, and
// a const array takes the pointer to const while a non-const one prefers the pointer to non-const.
#include <metal_stdlib>
using namespace metal;
float g(thread float *p) { return p[0]; }
float g(thread int *p) { return float(p[0]) * 10.0f; }
float h(thread float *p) { return 1.0f; }
float h(const thread float *p) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	float a[2] = { 1.0f, 2.0f };
	int b[2] = { 3, 4 };
	const float c[2] = { 5.0f, 6.0f };
	out[i] = g(a) + g(b) + h(a) + h(c);
}
