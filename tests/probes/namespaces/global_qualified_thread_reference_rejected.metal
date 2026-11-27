// EXPECT: error is not const but is bound to constant memory
#include <metal_stdlib>
using namespace metal;
constant float k = 1.0f;
kernel void kern(device float* out [[buffer(0)]]) {
	float k = 2.0f;
	thread float& r = ::k;
	out[0] = r;
}
