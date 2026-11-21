#include <metal_stdlib>
using namespace metal;
// RUN: grid=64 local=8 seed=1 cases=3
kernel void k(device const float* a [[buffer(0)]], device const float* b [[buffer(1)]],
	device float* o [[buffer(2)]], uint i [[thread_position_in_grid]]) {
	o[i] = a[i] * b[i] + 1.0f;
}
