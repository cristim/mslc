// EXPECT: error assigning to ".xy" of "ka", which is const or in constant memory
#include <metal_stdlib>
using namespace metal;
constant float4 ka[2] = { { 1.0f, 2.0f, 3.0f, 4.0f }, { 5.0f, 6.0f, 7.0f, 8.0f } };
kernel void kern(device float* out [[buffer(0)]]) {
	float4 ka[2] = { float4(0.0f), float4(0.0f) };
	::ka[0].xy = 5.0f;
	out[0] = ka[0].x;
}
