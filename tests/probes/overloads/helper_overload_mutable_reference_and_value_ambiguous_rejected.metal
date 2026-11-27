// EXPECT: error call to "f" is ambiguous
// A variable binds to the reference and copies to the value equally well (Apple: ambiguous).
#include <metal_stdlib>
using namespace metal;
float f(thread float &x) { return 1.0f; }
float f(float x) { return 2.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	float a = 1.0f;
	out[i] = f(a);
}
