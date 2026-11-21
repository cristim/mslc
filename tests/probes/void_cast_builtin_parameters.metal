// EXPECT: valid
// (void)g; for a builtin parameter names it and evaluates nothing, as for any other unused parameter
// (#232 item e: once a "recognised but not lowered yet" refusal; every builtin mslc lowers works now).
#include <metal_stdlib>
using namespace metal;
kernel void k(device uint *o [[buffer(0)]], uint3 a [[thread_position_in_grid]],
	uint3 b [[thread_position_in_threadgroup]], uint3 c [[threadgroup_position_in_grid]],
	uint d [[thread_index_in_threadgroup]], uint3 e [[threads_per_threadgroup]]) {
	(void)a; (void)b; (void)c; (void)d; (void)e;
	o[0] = 1;
}
