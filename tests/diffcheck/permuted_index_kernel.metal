#include <metal_stdlib>
using namespace metal;
// Buffer indices that are neither contiguous nor in parameter order: the Apple side is keyed
// by the buffer index and the Vulkan side by the parameter position.
kernel void k(device const float* a [[buffer(5)]], device float* o [[buffer(1)]],
	device const float* b [[buffer(3)]], uint i [[thread_position_in_grid]]) {
	o[i] = a[i] - b[i];
}
