// EXPECT: valid
// A mutable reference takes a variable of exactly its type, which beats a scalar's splat into
// a float3 (Apple: f(a) is the reference), and an rvalue cannot bind to it (f(1.0f) is float3).
#include <metal_stdlib>
using namespace metal;
float f(thread float &x) { x += 1.0f; return 1.0f; }
float f(float3 x) { return 10.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	float a = 1.0f;
	out[i] = f(a) + f(1.0f) + a;
}
