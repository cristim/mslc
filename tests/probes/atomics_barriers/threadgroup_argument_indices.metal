// EXPECT: valid
// REFLECT: { "kind": "ThreadgroupMemory", "metal_index": 2, "spec_id": 3, "element_size": 4, "param_index": 1, "name": "a" }
// REFLECT: { "kind": "ThreadgroupMemory", "metal_index": 0, "spec_id": 4, "element_size": 4, "param_index": 2, "name": "b" }
// REFLECT: { "kind": "ThreadgroupMemory", "metal_index": 1, "spec_id": 5, "element_size": 16, "param_index": 3, "name": "c" }
// REFLECT: { "kind": "Buffer", "metal_index": 0,
// DISASM-MATCH: OpTypePointer Workgroup %_arr_v4float_[A-Za-z0-9_]+
//
// Apple takes the indices in any order, numbers the threadgroup arguments apart from the
// buffers, and gives one with no attribute the lowest index no other argument reserved
// (checked with `metal -S -emit-llvm`: air.location_index 2 for "a" here, 1 for "c").
// The SpecId follows declaration order, not the Metal index.
kernel void threadgroup_argument_indices(device float* out [[buffer(0)]],
                                         threadgroup float* a [[threadgroup(2)]],
                                         threadgroup uint* b [[threadgroup(0)]],
                                         threadgroup float4* c [[threadgroup(1)]],
                                         uint l [[thread_position_in_threadgroup]])
{
	a[l] = 1.0;
	b[l] = 2u;
	c[l] = float4(3.0);
	threadgroup_barrier(mem_flags::mem_threadgroup);
	out[l] = a[l] + float(b[l]) + c[l].x;
}
