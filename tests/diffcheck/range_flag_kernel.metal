#include <metal_stdlib>
using namespace metal;
// RUN: grid=32 local=8 seed=5 cases=2
kernel void k(device const uint* idx [[buffer(0)]], device const float* table [[buffer(1)]],
	device float* o [[buffer(2)]], uint i [[thread_position_in_grid]]) {
	o[i] = table[idx[i]];
}
