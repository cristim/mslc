// EXPECT: error cannot determine what the reference "r" names
#include <metal_stdlib>
using namespace metal;
constant float k = 1.0f;
kernel void kern(device float* out [[buffer(0)]]) {
	float k = 2.0f;
	const thread float& r = ::k;
	out[0] = r;
}
