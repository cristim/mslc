// EXPECT: valid
// RUN: grid=4 local=4 cases=3
#include <metal_stdlib>
using namespace metal;
constant float ka[4] = { 10.0f, 20.0f, 30.0f, 40.0f };
kernel void kern(device float* out [[buffer(0)]], constant uint* idx [[buffer(1)]],
	uint gid [[thread_position_in_grid]]) {
	float ka[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
	const constant float& r = ::ka[1];
	const constant float& r2 = ::ka[idx[gid] & 3u];
	out[gid] = r + r2 + ka[gid & 3u];
}
