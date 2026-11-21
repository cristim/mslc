#include <metal_stdlib>
using namespace metal;
// The lanes of a bool vector stored to a buffer keep their order.
kernel void k(device const float4* a [[buffer(0)]], device bool4* o [[buffer(1)]],
	uint i [[thread_position_in_grid]]) {
	o[i] = a[i] > float4(0.0f);
}
