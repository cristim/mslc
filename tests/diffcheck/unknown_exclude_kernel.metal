#include <metal_stdlib>
using namespace metal;
// DIFFCHECK-EXCLUDE: scratchh a typo: no buffer has this name, so the directive would do nothing
kernel void k(device const float* a [[buffer(0)]], device float* o [[buffer(1)]],
	uint i [[thread_position_in_grid]]) {
	o[i] = a[i] + 1.0f;
}
