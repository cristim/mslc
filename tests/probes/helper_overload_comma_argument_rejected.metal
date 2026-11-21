// EXPECT: error a conditional or comma expression as an argument chooses among the overloads
#include <metal_stdlib>
using namespace metal;
float f(thread float &x) { x = 9.0f; return 1.0f; }
float f(int x) { return 2.0f; }
kernel void k(device float *out [[buffer(0)]]) {
	float la = 1.0f; float lb = 2.0f;
	out[0] = f((la, lb));
}
