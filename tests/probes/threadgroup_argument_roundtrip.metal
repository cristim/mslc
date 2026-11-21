// EXPECT: valid
// RUN: grid=64 local=8 cases=2
// The repro of #373: a kernel stores through a threadgroup memory argument and reads it back. diffcheck
// sets the length to one element per thread on Apple (setThreadgroupMemoryLength) and the element count
// in the argument's spec constant on Vulkan.
kernel void threadgroup_argument_roundtrip(device float* out [[buffer(0)]],
                                           threadgroup float* tg [[threadgroup(0)]],
                                           uint l [[thread_position_in_threadgroup]],
                                           uint g [[thread_position_in_grid]])
{
	tg[l] = 1.0;
	out[g] = tg[l];
}
