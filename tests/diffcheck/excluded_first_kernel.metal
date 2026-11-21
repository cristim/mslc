#include <metal_stdlib>
using namespace metal;
// DIFFCHECK-EXCLUDE: junk written with a thread-dependent value that the test ignores
kernel void k(device const float* a [[buffer(0)]], device float* junk [[buffer(1)]],
	device float* o [[buffer(2)]], uint i [[thread_position_in_grid]]) {
	junk[i] = a[i];
	o[i] = a[i] + 1.0f;
}
