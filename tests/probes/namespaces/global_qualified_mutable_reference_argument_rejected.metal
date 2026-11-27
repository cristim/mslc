// EXPECT: error is const or in constant memory
#include <metal_stdlib>
using namespace metal;
constant float k = 1.0f;
void mut(thread float& x) { x = 9.0f; }
kernel void kern(device float* out [[buffer(0)]]) {
	float k = 2.0f;
	mut(::k);
	out[0] = k;
}
