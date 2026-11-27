// EXPECT: error cannot store to "k", a constant declared at file scope
#include <metal_stdlib>
using namespace metal;
constant float k = 1.0f;
kernel void kern(device float* out [[buffer(0)]]) {
	float k = 2.0f;
	++::k;
	out[0] = k;
}
