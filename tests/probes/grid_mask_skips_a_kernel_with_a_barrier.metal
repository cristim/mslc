// EXPECT: valid
// FLAG: --mask-grid-size
// A thread that returns before a barrier leaves the rest of its workgroup waiting, so a kernel with one is not masked
// DISASM-NOT: OpUGreaterThanEqual
// REFLECT-NOT: grid_size_spec_ids
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	threadgroup_barrier(mem_flags::mem_device);
	out[i] = i + 1;
}
