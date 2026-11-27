// EXPECT: valid
// REFLECT: { "kind": "ThreadgroupMemory", "metal_index": 1, "spec_id": 3, "element_size": 4, "param_index": 1, "name": "a" }
// REFLECT: { "kind": "ThreadgroupMemory", "metal_index": 0, "spec_id": 4, "element_size": 4, "param_index": 2, "name": "c" }
// DISASM-NOT: PhysicalStorageBuffer %_arr_float
//
// A threadgroup pointer with no attribute is accepted by Apple and takes the lowest free
// threadgroup index. It was lowered as a device buffer before, which read and wrote
// memory the host never allocated. It does not count among the buffer indices either.
kernel void threadgroup_argument_implicit_index(device float* out [[buffer(0)]],
                                                threadgroup float* a,
                                                threadgroup float* c [[threadgroup(0)]],
                                                device float* in [[buffer(1)]],
                                                uint l [[thread_position_in_threadgroup]])
{
	a[l] = in[l];
	c[l] = 2.0;
	out[l] = a[l] + c[l];
}
