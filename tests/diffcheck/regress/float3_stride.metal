#include <metal_stdlib>
using namespace metal;
// A float3 in a buffer steps 16 bytes, not 12. Historical bug class: valid SPIR-V that reads
// bytes the host never wrote.
kernel void k(device const float3* a [[buffer(0)]], device float3* o [[buffer(1)]],
	uint i [[thread_position_in_grid]]) {
	o[i] = a[i] * 2.0f;
}
