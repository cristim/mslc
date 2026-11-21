// RUN: grid=64 local=8 seed=1 cases=3
#include <metal_stdlib>
using namespace metal;

// Two threadgroup memory arguments with different element types and indices out of order. Each thread
// writes its own slot and reads a neighbour's after the barrier, so the result depends on the
// length the host set (one element per thread) and on which argument is which.
kernel void threadgroup_argument(device float* out [[buffer(0)]],
                                 threadgroup float* a [[threadgroup(1)]],
                                 threadgroup uint2* b [[threadgroup(0)]],
                                 uint l [[thread_position_in_threadgroup]],
                                 uint g [[thread_position_in_grid]])
{
	a[l] = float(g) * 0.5;
	b[l] = uint2(g, g * 3u);
	threadgroup_barrier(mem_flags::mem_threadgroup);
	out[g] = a[(l + 1u) % 8u] + float(b[(l + 3u) % 8u].y);
}
