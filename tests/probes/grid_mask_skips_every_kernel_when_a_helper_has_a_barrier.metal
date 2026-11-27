// EXPECT: valid
// FLAG: --mask-grid-size
// A barrier in a helper is a barrier of every kernel that may call it, and the source is not searched for who does
// DISASM-NOT: OpUGreaterThanEqual
// REFLECT-NOT: grid_size_spec_ids
void sync()
{
	threadgroup_barrier(mem_flags::mem_device);
}
kernel void k(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	sync();
	out[i] = i + 1;
}
