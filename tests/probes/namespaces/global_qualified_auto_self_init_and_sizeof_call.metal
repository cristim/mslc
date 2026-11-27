// EXPECT: valid
// RUN: grid=4 local=4 cases=3
#include <metal_stdlib>
using namespace metal;
constant float k = 3.0f;
short f() { return 1; }
kernel void kern(device float* out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
	auto k = ::k;
	float f = 2.0f;
	out[gid] = k + float(sizeof(::f())) + f + float(gid);
}
