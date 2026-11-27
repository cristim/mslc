// EXPECT: error cannot store through "ka"
#include <metal_stdlib>
using namespace metal;
constant float ka[2] = { 1.0f, 2.0f };
kernel void kern(device float* out [[buffer(0)]]) {
	float ka[2] = { 3.0f, 4.0f };
	::ka[0] = 5.0f;
	out[0] = ka[0];
}
