// EXPECT: error cannot initialize "float[1]" from "One"
#include <metal_stdlib>
using namespace metal;
struct One { float x; };
kernel void k(device float* out [[buffer(0)]]) {
	array<float, 1> a = One{ 1.0f };
	out[0] = a[0];
}
